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

int stop_mpx;

void stop() {
	stop_mpx = 1;
}

static void *control_pipe_worker(void *arg) {
	while (!stop_mpx) {
		poll_control_pipe();
		usleep(10000);
	}

	close_control_pipe();
	pthread_exit(NULL);
}


static inline void get_fixed_buffer(float *out, float *in, float *temp_buffer, size_t in_size, size_t wanted_size) {

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
	rds_af_t af = {0};
	// Use arrays to enforce max length for RDS text items
	char ps[9] = "Mpxgen";
	char rt[65] = "Mpxgen: FM Stereo and RDS encoder";
	char ptyn[9] = {0};
	char callsign[5] = {0};
	uint16_t pi = 0x1000;
	uint8_t pty = 0;
	uint8_t tp = 0;
	float ppm = 0;
	uint8_t mpx = 50;
	uint8_t wait = 1;

	size_t frames;

	// SRC
	SRC_STATE *src_state[2];
	SRC_DATA src_data[2];

	// buffers
	short *mpx_out;

	short *audio_in_buffer_short = NULL;
	float *audio_in_buffer = NULL;
	float *resampled_audio_in_buffer = NULL;
	float *mpx_buffer = NULL;
	float *resampled_mpx_buffer = NULL;

	pthread_attr_t attr;

	pthread_t control_pipe_thread;
	/*
	pthread_t audio_input_thread;
	pthread_t input_resample_thread;
	pthread_t output_resample_thread;
	pthread_t mpx_output_thread;
	*/

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
				pi = strtoul(optarg, NULL, 16);
				break;

			case 's': //ps
				strncpy(ps, optarg, 8);
				break;

			case 'r': //rt
				strncpy(rt, optarg, 64);
				break;

			case 'p': //pty
				pty = strtoul(optarg, NULL, 10);
				break;

			case 'T': //tp
				tp = strtoul(optarg, NULL, 10);
				break;

			case 'A': //af
				if (af.num_afs > MAX_AF) {
					fprintf(stderr, "AF list is too large.\n");
					return 1;
				} else {
					uint16_t freq = (uint16_t)(10*strtof(optarg, NULL));
					if (freq < 876 || freq > 1079) {
						fprintf(stderr, "Alternative Frequency has to be set in range of 87.6 MHz - 107.9 MHz\n");
						return 1;
					}
					af.af[af.num_afs] = freq-875;
				}
				af.num_afs++;
				break;

			case 'P': //ptyn
				strncpy(ptyn, optarg, 8);
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
				argv[0], mpx, wait, rds, pi, ps, rt, pty, tp);
				return 1;
		}
	}

	if (!audio_file[0] && !rds) {
		fprintf(stderr, "Nothing to do. Exiting.\n");
		return 1;
	}

	pthread_attr_init(&attr);

	// buffers
	mpx_buffer = malloc(NUM_MPX_FRAMES_IN * sizeof(float));
	resampled_mpx_buffer = malloc(NUM_MPX_FRAMES_OUT * sizeof(float));
	mpx_out = malloc(NUM_MPX_FRAMES_OUT * sizeof(short));

	// Gracefully stop the encoder on SIGINT or SIGTERM
	signal(SIGINT, stop);
	signal(SIGTERM, stop);

	if (output_file[0] == 0) {
		if ((open_output("alsa:default", 192000, 2)) < 0) {
			return -1;
		}
	} else {
		if ((open_output(output_file, 192000, 2)) < 0) {
			return -1;
		}
	}

	audio_in_buffer_short = malloc(NUM_AUDIO_FRAMES_IN * sizeof(short));
	audio_in_buffer = malloc(NUM_AUDIO_FRAMES_IN * sizeof(float));
	resampled_audio_in_buffer = malloc(NUM_AUDIO_FRAMES_OUT * sizeof(float));

	if (audio_file[0]) {
		unsigned int sample_rate;
		if (open_input(audio_file, wait, &sample_rate) < 0) goto exit;

		// SRC in (input -> MPX)
		src_data[0].input_frames = NUM_AUDIO_FRAMES_IN;
		src_data[0].output_frames = NUM_AUDIO_FRAMES_OUT;
		src_data[0].src_ratio = (sample_rate / (double)192000);
		src_data[0].data_in = audio_in_buffer;
		src_data[0].data_out = resampled_audio_in_buffer;
		src_data[0].end_of_input = 0;

		if (resampler_init(&src_state[0], 2) < 0) {
			fprintf(stderr, "Could not create input resampler.\n");
			goto exit;
		}

		src_data[1].input_frames = NUM_MPX_FRAMES_IN;
	} else {
		src_data[1].input_frames = NUM_RDS_FRAMES_IN;
	}

	// SRC out (MPX -> output)
	src_data[1].output_frames = NUM_MPX_FRAMES_OUT;
	src_data[1].src_ratio = (192000 / (double)MPX_SAMPLE_RATE) + (ppm / 1e6);
	src_data[1].data_in = mpx_buffer;
	src_data[1].data_out = resampled_mpx_buffer;
	src_data[1].end_of_input = 0;

	if (resampler_init(&src_state[1], 2) < 0) {
		fprintf(stderr, "Could not create output resampler.\n");
		goto exit;
	}

	// Initialize the baseband generator
	fm_mpx_init();
	set_output_volume(mpx);

	// Initialize the RDS modulator
	if (!rds) set_carrier_volume(1, 0);
	if (init_rds_encoder(pi, ps, rt, pty, tp, af, ptyn, callsign) < 0) goto exit;

	// Initialize the control pipe reader
	if(control_pipe[0]) {
		if(open_control_pipe(control_pipe) == 0) {
			fprintf(stderr, "Reading control commands on %s.\n", control_pipe);
			// Create control pipe polling worker
			if (pthread_create(&control_pipe_thread, &attr, control_pipe_worker, NULL) < 0) {
				fprintf(stderr, "Could not create control pipe thread.\n");
				goto exit;
			}
		} else {
			fprintf(stderr, "Failed to open control pipe: %s.\n", control_pipe);
		}
	}

	pthread_attr_destroy(&attr);

	for (;;) {
		// TODO: run these as separate threads
		if (audio_file[0]) {
			fprintf(stderr, "Input does not work for now.\n");
			break;
			//read_input(audio_in_buffer_short);
			//short2float(audio_in_buffer_short, audio_in_buffer, NUM_AUDIO_FRAMES_IN * 2);
			//if (resample(src_state[0], src_data[0], &frames) < 0) break;
			//fm_mpx_get_samples(resampled_audio_in_buffer, mpx_buffer);
		} else {
			fm_rds_get_samples(mpx_buffer);
		}

		if (resample(src_state[1], src_data[1], &frames) < 0) break;

		float2short(resampled_mpx_buffer, mpx_out, frames * 2);

		if ((write_output(mpx_out, frames)) < 0) printf("hello\n");//break;

		if (stop_mpx) {
			fprintf(stderr, "Stopping...\n");
			break;
		}
	}

exit:
	fm_mpx_exit();
	if (audio_file[0]) close_input();

	close_output();

	if (audio_file[0]) resampler_exit(src_state[0]);
	resampler_exit(src_state[1]);

	free(mpx_out);

	free(audio_in_buffer_short);
	free(audio_in_buffer);
	free(resampled_audio_in_buffer);
	free(mpx_buffer);
	free(resampled_mpx_buffer);

	return 0;
}
