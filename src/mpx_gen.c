/*
    mpxgen - FM multiplex encoder with Stereo and RDS
    Copyright (C) 2019 Anthony96922

    See https://github.com/Anthony96922/mpxgen
*/

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <samplerate.h>
#include <ao/ao.h>

#include "rds.h"
#include "fm_mpx.h"
#include "control_pipe.h"

#define DATA_SIZE 4096
#define OUTPUT_DATA_SIZE (DATA_SIZE * 2)

ao_device *device;
SRC_STATE *src_state;

static void terminate(int num)
{
    fm_mpx_close();
    close_control_pipe();
    ao_close(device);
    ao_shutdown();
    src_delete(src_state);
    exit(num);
}

int out_channels = 2;
float volume = 0;

int postprocess(float *inbuf, short *outbuf, size_t inbufsize) {
	int j = 0;

	for (int i = 0; i < inbufsize; i++) {
		if (inbuf[i] <= -1 || inbuf[i] >= 1) {
			fprintf(stderr, "overmodulation! (%.7f)\n", inbuf[i]);
			return 1;
		}
		// scale samples
		inbuf[i] *= 32767;
		// volume control
		inbuf[i] *= (volume / 100);

		if (out_channels == 2) {
			// stereo upmix
			outbuf[j] = outbuf[j+1] = inbuf[i];
			j += 2;
		} else {
			outbuf[i] = inbuf[i];
		}
	}
	return 0;
}

int generate_mpx(char *audio_file, char *output_file, int rds, uint16_t pi, char *ps, char *rt, int *af_array, float mpx, char *control_pipe, int pty, int tp) {
	// Catch only important signals
	for (int i = 0; i < 25; i++) {
		struct sigaction sa;

		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = terminate;
		sigaction(i, &sa, NULL);
	}

	// Data structures for baseband data
	float mpx_data[DATA_SIZE];
	float resample_out[DATA_SIZE];
	short dev_out[OUTPUT_DATA_SIZE];

	// AO
	ao_initialize();
	int ao_driver;
	ao_sample_format format;
	memset(&format, 0, sizeof(format));
	format.bits = 16;
	format.channels = 2;
	format.rate = 192000;
	format.byte_format = AO_FMT_LITTLE;

	if (output_file != NULL) {
		ao_driver = ao_driver_id("raw");
		out_channels = 1;
		format.channels = 1;
		if ((device = ao_open_file(ao_driver, output_file, 1, &format, NULL)) == NULL) {
			fprintf(stderr, "Error: cannot open output file.\n");
			return 1;
		}
	} else {
		ao_driver = ao_driver_id("alsa");
		if ((device = ao_open_live(ao_driver, &format, NULL)) == NULL) {
			fprintf(stderr, "Error: cannot open sound device.\n");
			return 1;
		}
	}

	// SRC
	int src_error;
	size_t generated_frames;

	SRC_DATA src_data;
	src_data.src_ratio = 192000. / 228000;
	src_data.input_frames = DATA_SIZE;
	src_data.output_frames = OUTPUT_DATA_SIZE;
	src_data.data_out = resample_out;

	if ((src_state = src_new(SRC_SINC_MEDIUM_QUALITY, 1, &src_error)) == NULL) {
		fprintf(stderr, "Error: src_new failed: %s\n", src_strerror(src_error));
		return 1;
	}

	// Initialize the baseband generator
	if(fm_mpx_open(audio_file, DATA_SIZE, rds) < 0) return 1;

	// Initialize the RDS modulator
	if(rds) {
		rds_encoder_init(DATA_SIZE, pi, ps, rt, pty, tp);
		printf("RDS Options:\n");
		printf("PI: %04X, PS: \"%s\", PTY: %d\n", pi, ps, pty);
		printf("RT: \"%s\"\n", rt);
		if(af_array[0]) {
			set_rds_af(af_array);
			printf("AF: ");
			for(int f = 1; f < af_array[0]+1; f++) {
				printf("%.1f MHz ", (float)(af_array[f]+875)/10);
			}
			printf("\n");
		}
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

	volume = mpx;

	for (;;) {
		if(control_pipe) poll_control_pipe();

		if (fm_mpx_get_samples(mpx_data) < 0) break;
		src_data.data_in = mpx_data;

		if ((src_error = src_process(src_state, &src_data))) {
			fprintf(stderr, "Error: src_process failed: %s\n", src_strerror(src_error));
			break;
		}

		generated_frames = src_data.output_frames_gen;
		if (postprocess(resample_out, dev_out, generated_frames)) break;
		// num_bytes = generated_frames * channels * bytes per sample
		if (!ao_play(device, (char *)dev_out, generated_frames * out_channels * 2)) {
			fprintf(stderr, "Error: could not play audio.\n");
			break;
		}
	}

	return 0;
}

int main(int argc, char **argv) {
	int opt;
	char *audio_file = NULL;
	char *output_file = NULL;
	char *control_pipe = NULL;
	int rds = 1;
	int alternative_freq[100] = {0};
	int af_size = 0;
	char *ps = "mpxgen";
	char *rt = "mpxgen: FM Stereo and RDS encoder";
	uint16_t pi = 0xFFFF;
	int pty = 0;
	int tp = 0;
	float mpx = 100;

	const char	*short_opt = "a:o:m:R:i:s:r:p:T:A:C:h";
	struct option	long_opt[] =
	{
		{"audio", 	required_argument, NULL, 'a'},
		{"output-file",	required_argument, NULL, 'o'},
		{"mpx",		required_argument, NULL, 'm'},

		{"rds", 	required_argument, NULL, 'R'},
		{"pi",		required_argument, NULL, 'i'},
		{"ps",		required_argument, NULL, 's'},
		{"rt",		required_argument, NULL, 'r'},
		{"pty",		required_argument, NULL, 'p'},
		{"tp",		required_argument, NULL, 'T'},
		{"af",		required_argument, NULL, 'A'},
		{"ctl",		required_argument, NULL, 'C'},

		{"help",	no_argument, NULL, 'h'},
		{ 0,		0,		0,	0 }
	};

	while((opt = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1)
	{
		switch(opt)
		{
			case 'a': //audio
				audio_file = optarg;
				break;

			case 'o': //output-file
				output_file = optarg;
				break;

			case 'm': //mpx
				mpx = atoi(optarg);
				if (mpx < 1 || mpx > 100) {
					fprintf(stderr, "MPX volume must be between 1 - 100.\n");
					return 1;
				}
				break;

			case 'R': //rds
				rds = atoi(optarg);
				break;

			case 'i': //pi
				pi = (uint16_t) strtol(optarg, NULL, 16);
				break;

			case 's': //ps
				ps = optarg;
				break;

			case 'r': //rt
				rt = optarg;
				break;

			case 'p': //pty
				pty = atoi(optarg);
				if (pty < 0 || pty > 31) {
					fprintf(stderr, "PTY must be between 0 - 31.\n");
					return 1;
				}
				break;

			case 'T': //tp
				tp = atoi(optarg);
				break;

			case 'A': //af
				af_size++;
				alternative_freq[af_size] = (int)(10*atof(optarg))-875;
				if(alternative_freq[af_size] < 1 || alternative_freq[af_size] > 204)
					fprintf(stderr, "Alternative Frequency has to be set in range of 87.6 MHz - 107.9 MHz\n");
				break;

			case 'C': //ctl
				control_pipe = optarg;
				break;

			case 'h': //help
				fprintf(stderr, "Help: %s\n"
				      "	[--audio (-a) file] [--output-file (-o) PCM out] [--mpx (-m) mpx-volume]\n"
				      "	[--rds rds-switch] [--pi pi-code] [--ps ps-text]\n"
				      "	[--rt radiotext] [--tp traffic-program] [--pty program-type]\n"
				      "	[--af alternative-freq] [--ctl (-C) control-pipe]\n", argv[0]);
				return 1;
				break;

			default:
				fprintf(stderr, "(See -h / --help)\n");
				return 1;
				break;
		}
	}

	if (audio_file == NULL && !rds) {
		fprintf(stderr, "Nothing to encode. Exiting.\n");
		return 0;
	}

	alternative_freq[0] = af_size;

	int errcode = generate_mpx(audio_file, output_file, rds, pi, ps, rt, alternative_freq, mpx, control_pipe, pty, tp);

	terminate(errcode);
}
