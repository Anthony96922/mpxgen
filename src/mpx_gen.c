/*
 * mpxgen - FM multiplex encoder with Stereo and RDS
 * Copyright (C) 2019 Anthony96922
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "rds.h"
#include "fm_mpx.h"
#include "control_pipe.h"
#include "audio_conversion.h"
#include "resampler.h"
#include "input.h"
#include "output.h"

// buffers
static float *audio_in_buffer;
static float *resampled_audio_in_buffer;
static float *mpx_buffer;
static float *resampled_mpx_buffer;

// pthread
static pthread_t control_pipe_thread;
static pthread_t audio_input_thread;
static pthread_t input_resampler_thread;
static pthread_t mpx_thread;
static pthread_t rds_thread;
static pthread_t output_resampler_thread;
static pthread_t audio_output_thread;

static pthread_mutex_t control_pipe_mutex;
static pthread_mutex_t audio_input_mutex;
static pthread_mutex_t input_resampler_mutex;
static pthread_mutex_t mpx_mutex;
static pthread_mutex_t rds_mutex;
static pthread_mutex_t output_resampler_mutex;
static pthread_mutex_t audio_output_mutex;

static pthread_cond_t control_pipe_cond;
static pthread_cond_t audio_input_cond;
static pthread_cond_t input_resampler_cond;
static pthread_cond_t mpx_cond;
static pthread_cond_t rds_cond;
static pthread_cond_t output_resampler_cond;
static pthread_cond_t audio_output_cond;

static int stop_mpx;

static void stop() {
	stop_mpx = 1;
}

static void shutdown() {
	fprintf(stderr, "Exiting...\n");
	exit(2);
}

static void free_and_shutdown() {
	fprintf(stderr, "Freeing buffers...\n");
	if (mpx_buffer != NULL) free(mpx_buffer);
	if (resampled_mpx_buffer != NULL) free(resampled_mpx_buffer);
	if (audio_in_buffer != NULL) free(audio_in_buffer);
	if (resampled_audio_in_buffer != NULL) free(resampled_audio_in_buffer);
	shutdown();
}

// structs for the threads
typedef struct resample_thread_args_t {
	SRC_STATE **state;
	SRC_DATA data;
	float *in;
	float *out;
	size_t frames_in;
	size_t frames_out;
} resample_thread_args_t;

typedef struct audio_io_thread_args_t {
	float *data;
	size_t frames;
} audio_io_thread_args_t;

typedef struct mpx_thread_args_t {
	float *in;
	float *out;
	size_t frames;
} mpx_thread_args_t;

// threads
static void *control_pipe_worker(void *arg) {
	while (!stop_mpx) {
		poll_control_pipe();
		usleep(10000);
	}

	close_control_pipe();
	pthread_exit(NULL);
}

static void *audio_input_worker(void *arg) {
	int r;
	short buf[NUM_AUDIO_FRAMES_IN*2];
	audio_io_thread_args_t *args = (audio_io_thread_args_t *)arg;

	while (!stop_mpx) {
		r = read_input(buf);
		if (r < 0) break;
		short2float(buf, args->data, args->frames * 2);
		pthread_cond_signal(&input_resampler_cond);
	}

	pthread_exit(NULL);
}

// memcpy for copying float frames
#define floatf_memcpy(x, y, z) memcpy(x, y, z * 2 * sizeof(float))

static void *input_resampler_worker(void *arg) {
	int r;
	static float outbuf[NUM_AUDIO_FRAMES_OUT*2];
	static float leftoverbuf[NUM_AUDIO_FRAMES_OUT*4];
	static size_t outframes;
	static size_t total_outframes = 0;
	static size_t extra_frames;
	resample_thread_args_t *args = (resample_thread_args_t *)arg;
	args->data.data_in = args->in;
	args->data.data_out = outbuf;
	args->data.input_frames = args->frames_in;
	args->data.output_frames = args->frames_out;

	while (!stop_mpx) {

		pthread_cond_wait(&input_resampler_cond, &input_resampler_mutex);

		if (extra_frames) {
			floatf_memcpy(args->out, leftoverbuf, extra_frames);
			total_outframes += extra_frames;
			extra_frames = 0;
		}
		while (total_outframes < args->frames_out) {
			r = resample(*args->state, args->data, &outframes);
			if (r < 0) break;
			floatf_memcpy(args->data.data_out, outbuf + outframes, outframes);
			args->data.data_out += outframes*2;
			total_outframes += outframes;
		}
		if (total_outframes > args->frames_out) {
			extra_frames = total_outframes - args->frames_out;
			floatf_memcpy(args->out, args->data.data_out, args->frames_out);
			floatf_memcpy(leftoverbuf, args->data.data_out + extra_frames, extra_frames);
		}
		if (total_outframes == args->frames_out) {
			floatf_memcpy(args->out, args->data.data_out, args->frames_out);
		}
		args->data.data_out = args->out;
		total_outframes = 0;
	}

	pthread_exit(NULL);
}

static void *mpx_worker(void *arg) {
	mpx_thread_args_t *args = (mpx_thread_args_t *)arg;

	while (!stop_mpx) {
		pthread_cond_wait(&mpx_cond, &mpx_mutex);
		fm_mpx_get_samples(args->in, args->out);
		pthread_cond_signal(&output_resampler_cond);
	}

	pthread_exit(NULL);
}

static void *rds_worker(void *arg) {
	mpx_thread_args_t *args = (mpx_thread_args_t *)arg;

	while (!stop_mpx) {
		pthread_cond_wait(&rds_cond, &rds_mutex);
		fm_rds_get_samples(args->out);
		pthread_cond_signal(&output_resampler_cond);
	}


	pthread_exit(NULL);
}

static void *output_resampler_worker(void *arg) {
	int r;
	static float outbuf[NUM_MPX_FRAMES_OUT*2];
	static float leftoverbuf[NUM_MPX_FRAMES_OUT*4];
	static size_t outframes;
	static size_t total_outframes = 0;
	static size_t extra_frames;
	resample_thread_args_t *args = (resample_thread_args_t *)arg;
	args->data.data_in = args->in;
	args->data.data_out = outbuf;
	args->data.input_frames = args->frames_in;
	args->data.output_frames = args->frames_out;

	while (!stop_mpx) {

		pthread_cond_wait(&output_resampler_cond, &output_resampler_mutex);

		if (extra_frames) {
			floatf_memcpy(args->out, leftoverbuf, extra_frames);
			total_outframes += extra_frames;
			extra_frames = 0;
		}
		while (total_outframes < args->frames_out) {
			r = resample(*args->state, args->data, &outframes);
			if (r < 0) break;
			floatf_memcpy(args->data.data_out, outbuf + outframes, outframes);
			args->data.data_out += outframes*2;
			total_outframes += outframes;
		}
		if (total_outframes > args->frames_out) {
			extra_frames = total_outframes - args->frames_out;
			floatf_memcpy(args->out, args->data.data_out, args->frames_out);
			floatf_memcpy(leftoverbuf, args->data.data_out + extra_frames, extra_frames);
		}
		if (total_outframes == args->frames_out) {
			floatf_memcpy(args->out, args->data.data_out, args->frames_out);
		}
		args->data.data_out = args->out;
		total_outframes = 0;
	}

	pthread_exit(NULL);
}

static void *audio_output_worker(void *arg) {
	int r;
	short buf[NUM_MPX_FRAMES_OUT*2];
	audio_io_thread_args_t *args = (audio_io_thread_args_t *)arg;

	while (!stop_mpx) {
		pthread_cond_wait(&audio_output_cond, &audio_output_mutex);
		float2short(args->data, buf, args->frames * 2);
		r = write_output(buf, args->frames);
		if (r < 0) break;
	}

	pthread_exit(NULL);
}

void set_output_ppm(float new_ppm) {
	// no-op
}

int main(int argc, char **argv) {
	int opt;
	char audio_file[51] = {0};
	char output_file[51] = {0};
	char control_pipe[51] = {0};
	uint8_t rds = 1;
	rds_params_t rds_params = {
		.ps = "Mpxgen",
		.rt = "Mpxgen: FM Stereo and RDS encoder",
		.pi = 0x1000
	};
	char callsign[5] = {0};
	char tmp_ps[9] = {0};
	char tmp_rt[65] = {0};
	char tmp_ptyn[9] = {0};
	float ppm = 0;
	uint8_t mpx = 50;
	uint8_t wait = 1;

	int r;

	// SRC
	SRC_STATE *src_state[2];
	SRC_DATA src_data[2];

	int output_open_success = 0;

	// pthread
	pthread_attr_t attr;

	const char	*short_opt = "a:o:m:x:W:R:i:s:r:p:T:A:P:S:C:h";
	struct option	long_opt[] =
	{
		{"audio",	required_argument, NULL, 'a'},
		{"output-file",	required_argument, NULL, 'o'},

		{"mpx",		required_argument, NULL, 'm'},
		{"ppm",		required_argument, NULL, 'x'},
		{"wait",	required_argument, NULL, 'W'},

		{"rds",		required_argument, NULL, 'R'},
		{"pi",		required_argument, NULL, 'i'},
		{"ps",		required_argument, NULL, 's'},
		{"rt",		required_argument, NULL, 'r'},
		{"pty",		required_argument, NULL, 'p'},
		{"tp",		required_argument, NULL, 'T'},
		{"af",		required_argument, NULL, 'A'},
		{"ptyn",	required_argument, NULL, 'P'},
		{"callsign",	required_argument, NULL, 'S'},
		{"ctl",		required_argument, NULL, 'C'},

		{"help",	no_argument, NULL, 'h'},
		{ 0,		0,		0,	0 }
	};

	while((opt = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1)
	{
		switch(opt)
		{
			case 'a': //audio
				strncpy(audio_file, optarg, 50);
				break;

			case 'o': //output-file
				strncpy(output_file, optarg, 50);
				break;

			case 'm': //mpx
				mpx = strtoul(optarg, NULL, 10);
				if (mpx < 1 || mpx > 100) {
					fprintf(stderr, "MPX volume must be between 1 - 100.\n");
					return 1;
				}
				break;

			case 'x': //ppm
				ppm = strtof(optarg, NULL);
				break;

			case 'W': //wait
				wait = strtoul(optarg, NULL, 10);
				break;

			case 'R': //rds
				rds = strtoul(optarg, NULL, 10);
				break;

			case 'i': //pi
				rds_params.pi = strtoul(optarg, NULL, 16);
				break;

			case 's': //ps
				strncpy(tmp_ps, optarg, 8);
				memcpy(rds_params.ps, tmp_ps, 8);
				break;

			case 'r': //rt
				strncpy(tmp_rt, optarg, 64);
				memcpy(rds_params.rt, tmp_rt, 64);
				break;

			case 'p': //pty
				rds_params.pty = strtoul(optarg, NULL, 10);
				break;

			case 'T': //tp
				rds_params.tp = strtoul(optarg, NULL, 10);
				break;

			case 'A': //af
				if (rds_params.af.num_afs > MAX_AF) {
					fprintf(stderr, "AF list is too large.\n");
					return 1;
				} else {
					uint16_t freq = (uint16_t)(10*strtof(optarg, NULL));
					if (freq < 876 || freq > 1079) {
						fprintf(stderr, "Alternative Frequency has to be set in range of 87.6 MHz - 107.9 MHz\n");
						return 1;
					}
					rds_params.af.af[rds_params.af.num_afs] = freq-875;
				}
				rds_params.af.num_afs++;
				break;

			case 'P': //ptyn
				strncpy(tmp_ptyn, optarg, 8);
				memcpy(rds_params.ptyn, tmp_ptyn, 8);
				break;

			case 'S': //callsign
				strncpy(callsign, optarg, 4);
				break;

			case 'C': //ctl
				strncpy(control_pipe, optarg, 50);
				break;

			case 'h': //help
			case '?':
			default:
				fprintf(stderr,
					"This is Mpxgen, a lightweight Stereo and RDS encoder.\n"
					"\n"
					"Usage: %s [options]\n"
					"\n"
					"[Audio]\n"
					"\n"
					"    -a / --audio        Input file, pipe or ALSA capture\n"
					"    -o / --output-file  PCM out\n"
					"\n"
					"[MPX controls]\n"
					"\n"
					"    -m / --mpx          MPX volume [ default: %d ]\n"
					"    -x / --ppm          Clock drift correction\n"
					"    -W / --wait         Wait for new audio [ default: %d ]\n"
					"\n"
					"[RDS encoder]\n"
					"\n"
					"    -R / --rds          RDS switch [ default: %d ]\n"
					"    -i / --pi           Program Identification code [ default: %04X ]\n"
					"    -s / --ps           Program Service name [ default: \"%s\" ]\n"
					"    -r / --rt           Radio Text [ default: \"%s\" ]\n"
					"    -p / --pty          Program Type [ default: %d ]\n"
					"    -T / --tp           Traffic Program [ default: %d ]\n"
					"    -A / --af           Alternative Frequency (more than one AF may be passed)\n"
					"    -P / --ptyn         PTY Name\n"
					"    -S / --callsign     Callsign to calculate the PI code from (overrides -i/--pi)\n"
					"    -C / --ctl          Control pipe\n"
					"\n",
				argv[0], mpx, wait, rds, rds_params.pi, rds_params.ps, rds_params.rt, rds_params.pty, rds_params.tp);
				return 1;
		}
	}

	if (!audio_file[0] && !rds) {
		fprintf(stderr, "Nothing to do. Exiting.\n");
		return 1;
	}

	pthread_mutex_init(&control_pipe_mutex, NULL);
	pthread_mutex_init(&audio_input_mutex, NULL);
	pthread_mutex_init(&input_resampler_mutex, NULL);
	pthread_mutex_init(&mpx_mutex, NULL);
	pthread_mutex_init(&rds_mutex, NULL);
	pthread_mutex_init(&output_resampler_mutex, NULL);
	pthread_mutex_init(&audio_output_mutex, NULL);
	pthread_cond_init(&control_pipe_cond, NULL);
	pthread_cond_init(&audio_input_cond, NULL);
	pthread_cond_init(&input_resampler_cond, NULL);
	pthread_cond_init(&mpx_cond, NULL);
	pthread_cond_init(&rds_cond, NULL);
	pthread_cond_init(&output_resampler_cond, NULL);
	pthread_cond_init(&audio_output_cond, NULL);
	pthread_attr_init(&attr);

	// buffers
	mpx_buffer = malloc(NUM_MPX_FRAMES_IN * 2 * sizeof(float));
	resampled_mpx_buffer = malloc(NUM_MPX_FRAMES_OUT * 2 * sizeof(float));

	// Gracefully stop the encoder on SIGINT or SIGTERM
	signal(SIGINT, stop);
	signal(SIGTERM, stop);

	signal(SIGSEGV, free_and_shutdown);
	signal(SIGKILL, free_and_shutdown);

	if (output_file[0] == 0) {
		r = open_output("alsa:default", 192000, 2);
		if (r < 0) {
			goto free;
		}
		output_open_success = 1;
	} else {
		r = open_output(output_file, 192000, 2);
		if (r < 0) {
			goto free;
		}
		output_open_success = 1;
	}

	if (output_open_success) {
		audio_io_thread_args_t audio_output_thread_args;
		audio_output_thread_args.data = resampled_mpx_buffer;
		audio_output_thread_args.frames = NUM_MPX_FRAMES_OUT;
		// start output thread
		r = pthread_create(&audio_output_thread, &attr, audio_output_worker, (void *)&audio_output_thread_args);
		if (r < 0) {
			fprintf(stderr, "Could not create audio output thread.\n");
			goto exit;
		} else {
			fprintf(stderr, "Created output thread.\n");
		}
	}

	usleep(10000);

	if (audio_file[0]) {
		audio_in_buffer = malloc(NUM_AUDIO_FRAMES_IN * 2 * sizeof(float));
		resampled_audio_in_buffer = malloc(NUM_AUDIO_FRAMES_OUT * 2 * sizeof(float));

		unsigned int sample_rate;
		r = open_input(audio_file, wait, &sample_rate, NUM_AUDIO_FRAMES_IN);
		if (r < 0) goto free;

		// SRC in (input -> MPX)
		src_data[0].src_ratio = ((double)MPX_SAMPLE_RATE / sample_rate);

		r = resampler_init(&src_state[0], 2);
		if (r < 0) {
			fprintf(stderr, "Could not create input resampler.\n");
			goto exit;
		}

		resample_thread_args_t in_resampler_args;
		in_resampler_args.state = &src_state[0];
		in_resampler_args.data = src_data[0];
		in_resampler_args.in = audio_in_buffer;
		in_resampler_args.out = resampled_audio_in_buffer;
		in_resampler_args.frames_in = NUM_AUDIO_FRAMES_IN;
		in_resampler_args.frames_out = NUM_AUDIO_FRAMES_OUT;

		// start input resampler thread
		r = pthread_create(&input_resampler_thread, &attr, input_resampler_worker, (void *)&in_resampler_args);
		if (r < 0) {
			fprintf(stderr, "Could not create input resampler thread.\n");
			goto exit;
		} else {
			fprintf(stderr, "Created input resampler thread.\n");
		}

		usleep(10000);

		audio_io_thread_args_t audio_input_thread_args;
		audio_input_thread_args.data = audio_in_buffer;
		audio_input_thread_args.frames = NUM_AUDIO_FRAMES_IN;

		// start audio input thread
		r = pthread_create(&audio_input_thread, &attr, audio_input_worker, (void *)&audio_input_thread_args);
		if (r < 0) {
			fprintf(stderr, "Could not create file input thread.\n");
			goto exit;
		} else {
			fprintf(stderr, "Created file input thread.\n");
		}
	}

	// SRC out (MPX -> output)
	src_data[1].src_ratio = (192000 / (double)MPX_SAMPLE_RATE) + (ppm / 1e6);

	r = resampler_init(&src_state[1], 2);
	if (r < 0) {
		fprintf(stderr, "Could not create output resampler.\n");
		goto exit;
	} else {
		resample_thread_args_t out_resampler_args;
		out_resampler_args.state = &src_state[1];
		out_resampler_args.data = src_data[1];
		out_resampler_args.in = mpx_buffer;
		out_resampler_args.out = resampled_mpx_buffer;
		out_resampler_args.frames_in = NUM_MPX_FRAMES_IN;
		out_resampler_args.frames_out = NUM_MPX_FRAMES_OUT;

		usleep(10000);

		// start output resampler thread
		r = pthread_create(&output_resampler_thread, &attr, output_resampler_worker, (void *)&out_resampler_args);
		if (r < 0) {
			fprintf(stderr, "Could not create output resampler thread.\n");
			goto exit;
		} else {
			fprintf(stderr, "Created output resampler thread.\n");
		}
	}

	// Initialize the baseband generator
	fm_mpx_init();
	set_output_volume(mpx);

	// Initialize the RDS modulator
	if (!rds) set_carrier_volume(1, 0);
	if (init_rds_encoder(rds_params, callsign) < 0) goto exit;

	// Initialize the control pipe reader
	if(control_pipe[0]) {
		if(open_control_pipe(control_pipe) == 0) {
			fprintf(stderr, "Reading control commands on %s.\n", control_pipe);
			// Create control pipe polling worker
			r = pthread_create(&control_pipe_thread, &attr, control_pipe_worker, NULL);
			if (r < 0) {
				fprintf(stderr, "Could not create control pipe thread.\n");
				goto exit;
			} else {
				fprintf(stderr, "Created control pipe thread.\n");
			}
		} else {
			fprintf(stderr, "Failed to open control pipe: %s.\n", control_pipe);
		}
	}

	usleep(10000);

	// start MPX thread
	mpx_thread_args_t mpx_thread_args;
	mpx_thread_args.out = mpx_buffer;
	mpx_thread_args.frames = NUM_MPX_FRAMES_IN;
	if (audio_file[0]) {
		mpx_thread_args.in = resampled_audio_in_buffer;
		r = pthread_create(&mpx_thread, &attr, mpx_worker, (void *)&mpx_thread_args);
		if (r < 0) {
			fprintf(stderr, "Could not create MPX thread.\n");
			goto exit;
		} else {
			fprintf(stderr, "Created MPX thread.\n");
		}
	} else {
		r = pthread_create(&rds_thread, &attr, rds_worker, (void *)&mpx_thread_args);
		if (r < 0) {
			fprintf(stderr, "Could not create RDS thread.\n");
			goto exit;
		} else {
			fprintf(stderr, "Created RDS thread.\n");
		}
	}

	pthread_attr_destroy(&attr);

	if (audio_file[0]) {
		pthread_cond_signal(&mpx_cond);
	} else {
		pthread_cond_signal(&rds_cond);
	}

	for (;;) {
		if (stop_mpx) {
			fprintf(stderr, "Stopping...\n");
			break;
		}
		usleep(10000);
	}

exit:
	// shut down threads
	fprintf(stderr, "Waiting for threads to shut down.\n");
	pthread_join(control_pipe_thread, NULL);
	pthread_join(audio_input_thread, NULL);
	pthread_join(input_resampler_thread, NULL);
	pthread_join(mpx_thread, NULL);
	pthread_join(rds_thread, NULL);
	pthread_join(output_resampler_thread, NULL);
	pthread_join(audio_output_thread, NULL);

	fm_mpx_exit();
	if (audio_file[0]) close_input();

	close_output();

	if (audio_file[0]) resampler_exit(src_state[0]);
	resampler_exit(src_state[1]);

free:
	if (audio_file[0]) {
		free(audio_in_buffer);
		free(resampled_audio_in_buffer);
	}
	if (mpx_buffer != NULL) free(mpx_buffer);
	if (resampled_mpx_buffer != NULL) free(resampled_mpx_buffer);

	return 0;
}
