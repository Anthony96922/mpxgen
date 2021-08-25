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

#include "common.h"
#include <signal.h>
#include <getopt.h>
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
static float *out_buffer;

// pthread
static pthread_t control_pipe_thread;
static pthread_t input_thread;
static pthread_t resampler_thread;
static pthread_t mpx_thread;
static pthread_t rds_thread;
static pthread_t output_thread;

static pthread_mutex_t control_pipe_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t input_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t resampler_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mpx_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t rds_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t control_pipe_cond;
static pthread_cond_t input_cond;
static pthread_cond_t resampler_cond;
static pthread_cond_t mpx_cond;
static pthread_cond_t rds_cond;
static pthread_cond_t output_cond;

static uint8_t stop_mpx;

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
	if (out_buffer != NULL) free(out_buffer);
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
	double ratio;
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

static void *input_worker(void *arg) {
	int8_t r;
	short buf[NUM_AUDIO_FRAMES_IN*2];
	audio_io_thread_args_t *args = (audio_io_thread_args_t *)arg;
	float *audio = args->data;
	size_t frames = args->frames;

	while (!stop_mpx) {
		r = read_input(buf);
		if (r < 0) break;
		short2float(buf, audio, frames*2);
		pthread_cond_signal(&resampler_cond);
	}

	pthread_exit(NULL);
}

// memcpy for copying float frames
#define floatf_memcpy(x, y, z) memcpy(x, y, z * 2 * sizeof(float))

static void *resampler_worker(void *arg) {
	int8_t r;
	static float outbuf[NUM_AUDIO_FRAMES_OUT*2];
	static float leftoverbuf[NUM_AUDIO_FRAMES_OUT*2];
	static size_t outframes;
	static size_t total_outframes;
	static size_t extra_frames;

	struct resample_thread_args_t *args = (struct resample_thread_args_t *)arg;

	float *in = args->in;
	float *out = args->out;
	size_t frames_in = args->frames_in;
	size_t frames_out = args->frames_out;
	SRC_STATE *src_state = *args->state;
	SRC_DATA src_data = args->data;
	src_data.data_in = in;
	src_data.data_out = outbuf;
	src_data.input_frames = frames_in;
	src_data.output_frames = frames_out;
	src_data.src_ratio = args->ratio;

	while (!stop_mpx) {
		pthread_cond_wait(&resampler_cond, &resampler_mutex);
		if (extra_frames) {
			floatf_memcpy(out, leftoverbuf, extra_frames);
			total_outframes += extra_frames;
			extra_frames = 0;
		}
		while (total_outframes < frames_out) {
			r = resample(src_state, src_data, &outframes);
			if (r < 0) break;
			floatf_memcpy(src_data.data_out, outbuf + outframes, outframes);
			src_data.data_out += outframes*2;
			total_outframes += outframes;
		}
		// when there are excess frames
		if (total_outframes > frames_out) {
			extra_frames = total_outframes - frames_out;
			floatf_memcpy(out, src_data.data_out, frames_out);
			floatf_memcpy(leftoverbuf, src_data.data_out + extra_frames, extra_frames);
		}
		if (total_outframes == frames_out) {
			floatf_memcpy(out, src_data.data_out, frames_out);
		}
		src_data.data_out = out;
		total_outframes = 0;
	}

	pthread_exit(NULL);
}

static void *mpx_worker(void *arg) {
	struct mpx_thread_args_t *args = (struct mpx_thread_args_t *)arg;
	float *audio_in = args->in;
	float *mpx_out = args->out;

	while (!stop_mpx) {
		pthread_cond_wait(&mpx_cond, &mpx_mutex);
		fm_mpx_get_samples(audio_in, mpx_out);
		pthread_cond_signal(&output_cond);
	}

	pthread_exit(NULL);
}

static void *rds_worker(void *arg) {
	struct mpx_thread_args_t *args = (struct mpx_thread_args_t *)arg;
	float *rds_out = args->out;

	while (!stop_mpx) {
		//pthread_cond_wait(&rds_cond, &rds_mutex);
		fm_rds_get_samples(rds_out);
		//pthread_cond_signal(&output_cond);
	}


	pthread_exit(NULL);
}

static void *output_worker(void *arg) {
	int8_t r;
	static short buf[NUM_MPX_FRAMES*2];
	struct audio_io_thread_args_t *args = (struct audio_io_thread_args_t *)arg;
	size_t frames = args->frames;
	float *audio = args->data;

	while (!stop_mpx) {
		//pthread_cond_wait(&output_cond, &output_mutex);
		float2short(audio, buf, frames*2);
		r = write_output(buf, frames);
		if (r < 0) {
			stop_mpx = 1;
			break;
		}
		pthread_cond_signal(&mpx_cond);
		pthread_cond_signal(&rds_cond);
	}

	pthread_exit(NULL);
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
	uint8_t mpx = 50;
	uint8_t wait = 1;

	int8_t r;

	// SRC
	SRC_STATE *src_state;
	SRC_DATA src_data;

	uint8_t output_open_success = 0;

	// pthread
	pthread_attr_t attr;

	const char	*short_opt = "a:o:m:W:R:i:s:r:p:T:A:P:S:C:h";
	struct option	long_opt[] =
	{
		{"audio",	required_argument, NULL, 'a'},
		{"output-file",	required_argument, NULL, 'o'},

		{"mpx",		required_argument, NULL, 'm'},
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
					"    -m / --mpx          MPX volume [default: %d]\n"
					"    -W / --wait         Wait for new audio [default: %d]\n"
					"\n"
					"[RDS encoder]\n"
					"\n"
					"    -R / --rds          RDS switch [default: %d]\n"
					"    -i / --pi           Program Identification code [default: %04X]\n"
					"    -s / --ps           Program Service name [default: \"%s\"]\n"
					"    -r / --rt           Radio Text [default: \"%s\"]\n"
					"    -p / --pty          Program Type [default: %d]\n"
					"    -T / --tp           Traffic Program [default: %d]\n"
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
	pthread_mutex_init(&input_mutex, NULL);
	pthread_mutex_init(&resampler_mutex, NULL);
	pthread_mutex_init(&mpx_mutex, NULL);
	pthread_mutex_init(&rds_mutex, NULL);
	pthread_mutex_init(&output_mutex, NULL);
	pthread_cond_init(&control_pipe_cond, NULL);
	pthread_cond_init(&input_cond, NULL);
	pthread_cond_init(&resampler_cond, NULL);
	pthread_cond_init(&mpx_cond, NULL);
	pthread_cond_init(&rds_cond, NULL);
	pthread_cond_init(&output_cond, NULL);
	pthread_attr_init(&attr);

	// buffers
	mpx_buffer = malloc(NUM_MPX_FRAMES*2*sizeof(float));
	out_buffer = malloc(NUM_MPX_FRAMES*2*sizeof(float));

	// Gracefully stop the encoder on SIGINT or SIGTERM
	signal(SIGINT, stop);
	signal(SIGTERM, stop);

	signal(SIGSEGV, free_and_shutdown);
	signal(SIGKILL, free_and_shutdown);

	if (output_file[0] == 0) {
		r = open_output("alsa:default", MPX_SAMPLE_RATE, 2);
		if (r < 0) {
			goto free;
		}
		output_open_success = 1;
	} else {
		r = open_output(output_file, MPX_SAMPLE_RATE, 2);
		if (r < 0) {
			goto free;
		}
		output_open_success = 1;
	}

	if (output_open_success) {
		struct audio_io_thread_args_t output_thread_args;
		output_thread_args.data = out_buffer;
		output_thread_args.frames = NUM_MPX_FRAMES;
		// start output thread
		r = pthread_create(&output_thread, &attr, output_worker, (void *)&output_thread_args);
		if (r < 0) {
			fprintf(stderr, "Could not create audio output thread.\n");
			goto exit;
		} else {
			fprintf(stderr, "Created output thread.\n");
		}
	}

	if (audio_file[0]) {
		audio_in_buffer = malloc(NUM_AUDIO_FRAMES_IN*2*sizeof(float));
		resampled_audio_in_buffer = malloc(NUM_AUDIO_FRAMES_OUT*2*sizeof(float));

		uint32_t sample_rate;
		r = open_input(audio_file, wait, &sample_rate, NUM_AUDIO_FRAMES_IN);
		if (r < 0) goto free;

		// SRC in (input -> MPX)
		r = resampler_init(&src_state, 2);
		if (r < 0) {
			fprintf(stderr, "Could not create input resampler.\n");
			goto exit;
		}

		memset(&src_data, 0, sizeof(src_data));

		struct resample_thread_args_t resampler_args;
		resampler_args.state = &src_state;
		resampler_args.data = src_data;
		resampler_args.in = audio_in_buffer;
		resampler_args.out = resampled_audio_in_buffer;
		resampler_args.frames_in = NUM_AUDIO_FRAMES_IN;
		resampler_args.frames_out = NUM_AUDIO_FRAMES_OUT;
		resampler_args.ratio = (float)MPX_SAMPLE_RATE / (float)sample_rate;

		// start input resampler thread
		r = pthread_create(&resampler_thread, &attr, resampler_worker, (void *)&resampler_args);
		if (r < 0) {
			fprintf(stderr, "Could not create input resampler thread.\n");
			goto exit;
		} else {
			fprintf(stderr, "Created input resampler thread.\n");
		}

		struct audio_io_thread_args_t input_thread_args;
		input_thread_args.data = audio_in_buffer;
		input_thread_args.frames = NUM_AUDIO_FRAMES_IN;

		// start audio input thread
		r = pthread_create(&input_thread, &attr, input_worker, (void *)&input_thread_args);
		if (r < 0) {
			fprintf(stderr, "Could not create file input thread.\n");
			goto exit;
		} else {
			fprintf(stderr, "Created file input thread.\n");
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

	// start MPX thread
	struct mpx_thread_args_t mpx_thread_args;
	mpx_thread_args.out = mpx_buffer;
	mpx_thread_args.frames = NUM_MPX_FRAMES;
	if (audio_file[0]) {
		mpx_thread_args.in = resampled_audio_in_buffer;
		r = pthread_create(&mpx_thread, &attr, mpx_worker, (void *)&mpx_thread_args);
		if (r < 0) {
			fprintf(stderr, "Could not create MPX thread.\n");
			goto exit;
		} else {
			fprintf(stderr, "Created MPX thread.\n");
		}
		pthread_cond_signal(&mpx_cond);
	} else {
		r = pthread_create(&rds_thread, &attr, rds_worker, (void *)&mpx_thread_args);
		if (r < 0) {
			fprintf(stderr, "Could not create RDS thread.\n");
			goto exit;
		} else {
			fprintf(stderr, "Created RDS thread.\n");
		}
		pthread_cond_signal(&rds_cond);
	}

	pthread_attr_destroy(&attr);

	for (;;) {
		if (stop_mpx) {
			fprintf(stderr, "Stopping...\n");
			break;
		}
		usleep(100000);
	}

exit:
	// shut down threads
	fprintf(stderr, "Waiting for threads to shut down.\n");
	pthread_cond_signal(&input_cond);
	pthread_cond_signal(&resampler_cond);
	pthread_cond_signal(&mpx_cond);
	pthread_cond_signal(&rds_cond);
	pthread_cond_signal(&output_cond);
	pthread_join(control_pipe_thread, NULL);
	pthread_join(input_thread, NULL);
	pthread_join(resampler_thread, NULL);
	pthread_join(mpx_thread, NULL);
	pthread_join(rds_thread, NULL);
	pthread_join(output_thread, NULL);

	fm_mpx_exit();
	if (audio_file[0]) close_input();

	close_output();

	if (audio_file[0]) resampler_exit(src_state);

free:
	if (audio_file[0]) {
		if (audio_in_buffer != NULL) free(audio_in_buffer);
		if (resampled_audio_in_buffer != NULL) free(resampled_audio_in_buffer);
	}
	if (mpx_buffer != NULL) free(mpx_buffer);
	if (out_buffer != NULL) free(out_buffer);

	return 0;
}
