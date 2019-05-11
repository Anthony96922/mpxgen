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
#include <sndfile.h>
#include <getopt.h>

#include "rds.h"
#include "fm_mpx.h"
#include "control_pipe.h"

#define NUM_SAMPLES			64000
#define SUBSIZE                         1
#define DATA_SIZE                       1000

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

int generate_mpx(char *audio_file, int rds, uint16_t pi, char *ps, char *rt, int *af_array, int preemphasis_cutoff, char *control_pipe, int pty, int tp, int wait) {
	// Catch only important signals
	for (int i = 0; i < 25; i++) {
		struct sigaction sa;

		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = terminate;
		sigaction(i, &sa, NULL);
	}

	// Data structures for baseband data
	double data[DATA_SIZE];
	double rds_buffer[DATA_SIZE];

	int out_channels = 2;
	double audio_out_buffer[DATA_SIZE * out_channels];

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

	SNDFILE *outf;
	SF_INFO sfinfo;

	sfinfo.frames = DATA_SIZE * out_channels;
	sfinfo.samplerate = 228000;
	sfinfo.channels = out_channels;
	sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
	sfinfo.sections = 1;
	sfinfo.seekable = 0;

	outf = sf_open("test.wav", SFM_WRITE, &sfinfo);

	int count, outcount = 0;

	for (;;) {
		if(control_pipe) poll_control_pipe();

		if (fm_mpx_get_samples(data, rds_buffer, rds, wait) < 0) break;

		// copy the mono channel to the two stereo channels
		for (count = 0; count < DATA_SIZE; count++) {
			audio_out_buffer[outcount] = audio_out_buffer[outcount+1] = data[count];
			outcount += 2;
		}

		outcount = 0;

		sf_write_double(outf, audio_out_buffer, DATA_SIZE * out_channels);

		usleep(DATA_SIZE * out_channels);
	}

	sf_close(outf);

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
	uint16_t pi = 0x0000;
	int preemphasis_cutoff = 0;
	int pty = 9;
	int tp = 0;
	int wait = 0;

	const char	*short_opt = "a:P:m:W:C:h";
	struct option	long_opt[] =
	{
		{"audio", 	required_argument, NULL, 'a'},
		{"preemph",	required_argument, NULL, 'P'},
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

	if (argc == 1) {
		printf("No options specified. See -h (--help)\n");
		return 1;
	}

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
				      "                [--preemph (-P) preemphasis] [--div (-D) divider] \n"
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

	int errcode = generate_mpx(audio_file, rds, pi, ps, rt, alternative_freq, preemphasis_cutoff, control_pipe, pty, tp, wait);

	terminate(errcode);
}
