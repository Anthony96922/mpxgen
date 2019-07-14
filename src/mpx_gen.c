/*

    PiFmAdv - Advanced FM transmitter for the Raspberry Pi
    Copyright (C) 2017 Miegl

    See https://github.com/Miegl/PiFmAdv
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <samplerate.h>
#include <ao/ao.h>

#include "rds.h"
#include "fm_mpx.h"
#include "control_pipe.h"

#define NUM_SAMPLES		65536
#define DATA_SIZE		4096
#define BUFFER_SIZE		8192
#define CHANNELS		2

static void terminate(int num)
{
    fm_mpx_close();
    close_control_pipe();

    printf("MPX generator stopped\n");

    exit(num);
}

static void fatal(char *fmt, ...)
{
    va_list ap;
    fprintf(stderr,"ERROR: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    terminate(0);
}

void stereoize(short *inbuf, short *outbuf, int inbufsize) {
	int j = 0;

	// copy the mono channel to the two stereo channels
	for (int i = 0; i < inbufsize; i++) {
		outbuf[j] = outbuf[j+1] = inbuf[i];
		j += 2;
	}
}

void float2pcm16(float *inbuf, short *outbuf, int inbufsize) {
	for (int i = 0; i < inbufsize; i++) {
		outbuf[i] = 32767 * inbuf[i];
	}
}

int generate_mpx(char *audio_file, int rds, uint16_t pi, char *ps, char *rt, int *af_array, int preemphasis_cutoff, int mpx, char *control_pipe, int pty, int tp, int wait) {
	// Catch only important signals
	for (int i = 0; i < 25; i++) {
		struct sigaction sa;

		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = terminate;
		sigaction(i, &sa, NULL);
	}

	// Data structures for baseband data
	float mpx_data[DATA_SIZE];
	float rds_data[DATA_SIZE];
	float resample_out[DATA_SIZE];
	short scale_out[BUFFER_SIZE];
	short dev_out[BUFFER_SIZE];

	// AO
	ao_device *device;
	ao_sample_format format;
	ao_initialize();
	int default_driver = ao_default_driver_id();
	memset(&format, 0, sizeof(format));
	format.bits = 16;
	format.channels = CHANNELS;
	format.rate = 192000;
	format.byte_format = AO_FMT_LITTLE;

	device = ao_open_live(default_driver, &format, NULL);
	if (device == NULL) {
		fprintf(stderr, "Error: cannot open sound device.\n");
		return 1;
	}

	// SRC
	int src_init_error;
	int src_error;
	int generated_frames;

	SRC_STATE *src_state;
	SRC_DATA src_data;

	if ((src_state = src_new(SRC_SINC_FASTEST, 1, &src_init_error)) == NULL) {
		printf("Error: src_new failed: %s\n", src_strerror(src_init_error));
		return 1;
	}

	src_data.end_of_input = 0;
	src_data.input_frames = 0;
	src_data.data_in = mpx_data;
	src_data.src_ratio = 192000. / 228000;
	src_data.data_out = resample_out;
	src_data.output_frames = DATA_SIZE;

	printf("Starting MPX generator\n");

	// Initialize the baseband generator
	if(fm_mpx_open(audio_file, DATA_SIZE, preemphasis_cutoff) < 0) return 1;

	// Initialize the RDS modulator
	set_rds_pi(pi);
	set_rds_ps(ps);
	set_rds_rt(rt);
	set_rds_pty(pty);
	set_rds_tp(tp);
	set_rds_ms(1);
	set_rds_ab(0);

	printf("RDS Options:\n");

	if(rds) {
		printf("RDS: %i, ", rds);
		printf("PI: %04X, PS: \"%s\", PTY: %i\n", pi, ps, pty);
		printf("RT: \"%s\"\n", rt);
		if(af_array[0]) {
			set_rds_af(af_array);
			printf("AF: ");
			int f;
			for(f = 1; f < af_array[0]+1; f++) {
				printf("%f Mhz ", (float)(af_array[f]+875)/10);
			}
			printf("\n");
		}
	}
	else {
		printf("RDS: %i\n", rds);
	}

	// Initialize the control pipe reader
	if(control_pipe) {
		if(open_control_pipe(control_pipe) == 0) {
			printf("Reading control commands on %s.\n", control_pipe);
		} else {
			printf("Failed to open control pipe: %s.\n", control_pipe);
			control_pipe = NULL;
		}
	}

	for (;;) {
		if(control_pipe) poll_control_pipe();

		if (fm_mpx_get_samples(mpx_data, rds_data, mpx, rds, wait) < 0) break;
		src_data.input_frames = DATA_SIZE;
		src_data.data_in = mpx_data;
		src_data.data_out = resample_out;

		if ((src_error = src_process(src_state, &src_data))) {
			printf("Error: src_process failed: %s\n", src_strerror(src_error));
			break;
		}

		generated_frames = src_data.output_frames_gen;
		float2pcm16(resample_out, scale_out, generated_frames);
		stereoize(scale_out, dev_out, generated_frames);
		ao_play(device, (char *)dev_out, generated_frames * CHANNELS * 2);
	}

	ao_close(device);
	ao_shutdown();

	return 0;
}

int main(int argc, char **argv) {
	int opt;

	char *audio_file = NULL;
	char *control_pipe = NULL;
    	int rds = 1;
	int alternative_freq[100] = {};
	int af_size = 0;
	char *ps = "MPXGEN";
	char *rt = "MPXGEN: FM multiplex generator and RDS encoder";
	uint16_t pi = 0x1234;
	int preemphasis_cutoff = 0;
	int pty = 0;
	int tp = 0;
	int mpx = 100;
	int wait = 0;

	const char	*short_opt = "a:P:m:W:C:h";
	struct option	long_opt[] =
	{
		{"audio", 	required_argument, NULL, 'a'},
		{"preemph",	required_argument, NULL, 'P'},
		{"mpx",		required_argument, NULL, 'm'},
		{"wait",	required_argument, NULL, 'W'},

		{"rds", 	required_argument, NULL, 'rds'},
		{"pi", 		required_argument, NULL, 'pi'},
		{"ps", 		required_argument, NULL, 'ps'},
		{"rt", 		required_argument, NULL, 'rt'},
		{"pty", 	required_argument, NULL, 'pty'},
		{"tp",		required_argument, NULL, 'tp'},
		{"af", 		required_argument, NULL, 'af'},
		{"ctl", 	required_argument, NULL, 'C'},

		{"help",	no_argument, NULL, 'h'},
		{ 0, 		0, 		   0,    0 }
	};

	while((opt = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1)
	{
		switch(opt)
		{
			case -1:
			case 0:
			break;

			case 'a': //audio
				audio_file = optarg;
				break;

			case 'P': //preemph
				if(strcmp("eu", optarg)==0) {
					preemphasis_cutoff = 3185;
				} else if(strcmp("us", optarg)==0) {
					preemphasis_cutoff = 2120;
				}
				else {
					preemphasis_cutoff = atoi(optarg);
				}
				break;

			case 'm': //mpx
				mpx = atoi(optarg);
				break;

			case 'W': //wait
				wait = atoi(optarg);
				break;

			case 'rds': //rds
				rds = atoi(optarg);
				break;

			case 'pi': //pi
				pi = (uint16_t) strtol(optarg, NULL, 16);
				break;

			case 'ps': //ps
				ps = optarg;
				break;

			case 'rt': //rt
				rt = optarg;
				break;

			case 'pty': //pty
				pty = atoi(optarg);
				break;

			case 'tp': //tp
				tp = atoi(optarg);
				break;

			case 'af': //af
				af_size++;
				alternative_freq[af_size] = (int)(10*atof(optarg))-875;
				if(alternative_freq[af_size] < 1 || alternative_freq[af_size] > 204)
					fatal("Alternative Frequency has to be set in range of 87.6 Mhz - 107.9 Mhz\n");
				break;

			case 'C': //ctl
				control_pipe = optarg;
				break;

			case 'h': //help
				fatal("Help:\n"
				      "Syntax: mpx_gen [--audio (-a) file]\n"
				      "                [--mpx (-m) mpx-power] [--preemph (-P) preemphasis] [--div (-D) divider]\n"
				      "                [--wait (-W) wait-switch]\n"
				      "                [--rds rds-switch] [--pi pi-code] [--ps ps-text] [--rt radiotext] [--tp traffic-program]\n"
				      "                [--pty program-type] [--af alternative-freq] [--ctl (-C) control-pipe]\n");

				break;

			case ':':
				fatal("%s: option '-%c' requires an argument\n", argv[0], optopt);
				break;

			case '?':
			default:
				fatal("%s: option '-%c' is invalid. See -h (--help)\n", argv[0], optopt);
				break;
		}
	}

	alternative_freq[0] = af_size;

	int errcode = generate_mpx(audio_file, rds, pi, ps, rt, alternative_freq, preemphasis_cutoff, mpx, control_pipe, pty, tp, wait);

	terminate(errcode);
}
