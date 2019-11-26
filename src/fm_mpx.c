/*
    mpxgen - FM multiplex encoder with Stereo and RDS
    Copyright (C) 2019 Anthony96922

    See https://github.com/Anthony96922/mpxgen
*/

#include <sndfile.h>
#include <stdlib.h>
#include <strings.h>
#include <math.h>

#include "rds.h"
#include "fm_mpx.h"

#define FIR_HALF_SIZE	64
#define FIR_SIZE	(2*FIR_HALF_SIZE-1)

// coefficients of the low-pass FIR filter
float low_pass_fir[FIR_HALF_SIZE];
float fir_buffer_left[FIR_SIZE];
float fir_buffer_right[FIR_SIZE];

int fir_index;

float carrier_19[] = {0, 0.5, 0.8660254, 1, 0.8660254, 0.5, 0, -0.5, -0.8660254, -1, -0.8660254, -0.5};
float carrier_38[] = {0, 0.8660254, 0.8660254, 0, -0.8660254, -0.8660254};
float carrier_57[] = {0, 1, 0, -1};
int phase_19;
int phase_38;
int phase_57;

size_t length;
float upsample_factor;

float *audio_buffer;
float *rds_buffer;

int audio_index;
int audio_len;
float audio_pos;

int channels;
int rds;

float level_19 = 1;
float level_38 = 1;
float level_57 = 1;

SNDFILE *inf;

float *alloc_empty_buffer(size_t length) {
    float *p = malloc(length * sizeof(float));
    if(p == NULL) return NULL;

    bzero(p, length * sizeof(float));

    return p;
}

int fm_mpx_open(char *filename, size_t len, int rds_on) {
	length = len;
	rds = rds_on;

	if(filename != NULL) {
		// Open the input file
		SF_INFO sfinfo;

		// stdin or file on the filesystem?
		if(filename[0] == '-') {
			if(!(inf = sf_open_fd(fileno(stdin), SFM_READ, &sfinfo, 0))) {
				fprintf(stderr, "Error: could not open stdin for audio input.\n");
				return -1;
			} else {
				printf("Using stdin for audio input.\n");
			}
		} else {
			if(!(inf = sf_open(filename, SFM_READ, &sfinfo))) {
				fprintf(stderr, "Error: could not open input file %s.\n", filename);
				return -1;
			} else {
				printf("Using audio file: %s\n", filename);
			}
		}

		int in_samplerate = sfinfo.samplerate;
		upsample_factor = 228000. / in_samplerate;
		channels = sfinfo.channels;

		printf("Input: %d Hz, %d channels, upsampling factor: %.2f\n", in_samplerate, channels, upsample_factor);

		int cutoff_freq = 17000;
		if(in_samplerate/2 < cutoff_freq) cutoff_freq = in_samplerate/2;

		// Here we divide this coefficient by two because it will be counted twice
		// when applying the filter
		low_pass_fir[FIR_HALF_SIZE-1] = 2 * cutoff_freq / 228000 /2;

		// Only store half of the filter since it is symmetric
		for(int i=1; i<FIR_HALF_SIZE; i++) {
			low_pass_fir[FIR_HALF_SIZE-1-i] =
				sin(2 * M_PI * cutoff_freq * i / 228000) / (M_PI * i) // sinc
				* (.54 - .46 * cos(2 * M_PI * (i+FIR_HALF_SIZE) / (2*FIR_HALF_SIZE))); // Hamming window
		}

		printf("Created low-pass FIR filter for audio channels, with cutoff at %d Hz\n", cutoff_freq);

		audio_pos = upsample_factor;
		audio_buffer = alloc_empty_buffer(length * channels);
		if(audio_buffer == NULL) return -1;
	}

	if (rds) {
		rds_buffer = alloc_empty_buffer(length);
		if (rds_buffer == NULL) return -1;
	}

	return 0;
}

int fm_mpx_get_samples(float *mpx_buffer) {

	if(rds) get_rds_samples(rds_buffer);

	if (inf == NULL) {
		for (int i=0; i<length; i++) {
			mpx_buffer[i] = rds_buffer[i] * carrier_57[phase_57++] * 0.05 * level_57;
			if (phase_57 == 4) phase_57 = 0;
		}
		return 0;
	}

	for(int i=0; i<length; i++) {
		if(audio_pos >= upsample_factor) {
			audio_pos -= upsample_factor;

			if(audio_len <= channels) {
				for(int j=0; j<2; j++) { // one retry
					audio_len = sf_read_float(inf, audio_buffer, length);
					if (audio_len < 0) {
						fprintf(stderr, "Error reading audio\n");
						return -1;
					} else if (audio_len == 0) {
						if( sf_seek(inf, 0, SEEK_SET) < 0 ) break;
					} else {
						break;
					}
				}
				audio_index = 0;
			} else {
				audio_index += channels;
				audio_len -= channels;
			}
		}
		audio_pos++;

		// First store the current sample(s) into the FIR filter's ring buffer
		fir_buffer_left[fir_index] = audio_buffer[audio_index];
		if (channels == 2) {
			fir_buffer_right[fir_index] = audio_buffer[audio_index+1];
		}
		fir_index++;
		if(fir_index == FIR_SIZE) fir_index = 0;

		// L/R signals
		float out_left = 0;
		float out_right = 0;

		// Now apply the FIR low-pass filter

		/* As the FIR filter is symmetric, we do not multiply all
		   the coefficients independently, but two-by-two, thus reducing
		   the total number of multiplications by a factor of two
		 */
		int ifbi = fir_index;  // ifbi = increasing FIR Buffer Index
		int dfbi = fir_index;  // dfbi = decreasing FIR Buffer Index
		for(int fi=0; fi<FIR_HALF_SIZE; fi++) {  // fi = Filter Index
			dfbi--;
			if(dfbi < 0) dfbi = FIR_SIZE-1;
			out_left += low_pass_fir[fi] * (fir_buffer_left[ifbi] + fir_buffer_left[dfbi]);
			if(channels == 2) {
				out_right += low_pass_fir[fi] * (fir_buffer_right[ifbi] + fir_buffer_right[dfbi]);
			}
			ifbi++;
			if(ifbi == FIR_SIZE) ifbi = 0;
		}
		// End of FIR filter

		// Create sum and difference signals
		float out_mono   = out_left + out_right;
		float out_stereo = out_left - out_right;

		if (channels == 2) {
			// audio signals need to be limited to 45% to remain within modulation limits
			mpx_buffer[i] = out_mono * 0.45 +
				carrier_19[phase_19++] * 0.05 * level_19 +
				carrier_38[phase_38++] * out_stereo * 0.45 * level_38;

			if (phase_19 == 12) phase_19 = 0;
			if (phase_38 == 6) phase_38 = 0;
		} else {
			// mono audio is limited to 90%
			mpx_buffer[i] = out_mono * 0.9;
		}

		if (rds) {
			mpx_buffer[i] +=
				carrier_57[phase_57++] * rds_buffer[i] * 0.05 * level_57;
		}

		if (phase_57 == 4) phase_57 = 0;
	}

	return 0;
}

void set_19_level(int new_level) {
	if (new_level == -1) return;
	level_19 = (new_level / 100.0);
}

void set_38_level(int new_level) {
	if (new_level == -1) return;
	level_38 = (new_level / 100.0);
}

void set_57_level(int new_level) {
	if (new_level == -1) return;
	level_57 = (new_level / 100.0);
}

void fm_mpx_close() {
	if(sf_close(inf)) fprintf(stderr, "Error closing audio file\n");

	if(audio_buffer != NULL) free(audio_buffer);
	if (rds_buffer != NULL) free(rds_buffer);
}
