/*
    mpxgen - FM multiplex encoder with Stereo and RDS
    Copyright (C) 2019 Anthony96922

    See https://github.com/Anthony96922/mpxgen
*/

#include <sndfile.h>
#include <stdlib.h>
#include <strings.h>
#include <math.h>
#include <string.h>

#include "rds.h"
#include "fm_mpx.h"

#define OLD_FILTER

#ifdef OLD_FILTER
#define FIR_HALF_SIZE	64
#define FIR_SIZE	(2*FIR_HALF_SIZE-1)

// coefficients of the low-pass FIR filter
float low_pass_fir[FIR_HALF_SIZE];
float fir_buffer_left[FIR_SIZE] = {0};
float fir_buffer_right[FIR_SIZE] = {0};
#else
#define FIR_PHASES	32
#define FIR_TAPS	32 // MUST be a power of 2 for the circular buffer

// coefficients of the low-pass FIR filter
float low_pass_fir[FIR_PHASES][FIR_TAPS] = {0};
float fir_buffer_left[FIR_TAPS] = {0};
float fir_buffer_right[FIR_TAPS] = {0};
float gain = 200;
#endif
int fir_index = 0;

float carrier_19[] = {0, 0.5, 0.8660254, 1, 0.8660254, 0.5, 0, -0.5, -0.8660254, -1, -0.8660254, -0.5};
float carrier_38[] = {0, 0.8660254, 0.8660254, 0, -0.8660254, -0.8660254};
float carrier_57[] = {0, 1, 0, -1};
int phase_19 = 0;
int phase_38 = 0;
int phase_57 = 0;

size_t length = 0;
float downsample_factor = 0;

float *audio_buffer;
int audio_index = 0;
int audio_len = 0;
float audio_pos = 0;

int channels = 0;

float rds_buffer[DATA_SIZE] = {0};
size_t buffer_size = sizeof(rds_buffer);

int rds = 0;

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
		downsample_factor = 228000. / in_samplerate;

		printf("Input: %d Hz, upsampling factor: %.2f\n", in_samplerate, downsample_factor);

		channels = sfinfo.channels;
		if(channels == 2) {
			printf("2 channels, generating stereo multiplex.\n");
		} else {
			printf("1 channel, monophonic operation.\n");
		}

		int cutoff_freq = 17000;
		if(in_samplerate/2 < cutoff_freq) cutoff_freq = in_samplerate/2;

#ifdef OLD_FILTER
		low_pass_fir[FIR_HALF_SIZE-1] = 2 * cutoff_freq / 228000 /2;
		// Here we divide this coefficient by two because it will be counted twice
		// when applying the filter

		// Only store half of the filter since it is symmetric
		for(int i=1; i<FIR_HALF_SIZE; i++) {
			low_pass_fir[FIR_HALF_SIZE-1-i] =
				sin(2 * M_PI * cutoff_freq * i / 228000) / (M_PI * i) // sinc
				* (.54 - .46 * cos(2*M_PI * (i+FIR_HALF_SIZE) / (2*FIR_HALF_SIZE))); // Hamming window
		}
#else
		// Create the low-pass FIR filter, with pre-emphasis
		double window, firlowpass, firpreemph, sincpos;

		// IIR pre-emphasis filter
		// Reference material: http://jontio.zapto.org/hda1/preempiir.pdf
		double tau = 1e-6; // 1us = disable preemphasis
		double delta = 1.96e-6;
		double taup, deltap, bp, ap, a0, a1, b1;
		taup = 1.0/(2.0*(in_samplerate*FIR_PHASES))/tan(1.0/(2*tau*(in_samplerate*FIR_PHASES)));
		deltap = 1.0/(2.0*(in_samplerate*FIR_PHASES))/tan(1.0/(2*delta*(in_samplerate*FIR_PHASES)));
		bp = sqrt(-taup*taup + sqrt(taup*taup*taup*taup + 8.0*taup*taup*deltap*deltap))/2.0;
		ap = sqrt(2*bp*bp + taup*taup);
		a0 = ( 2.0*ap + 1.0/(in_samplerate*FIR_PHASES))/(2.0*bp + 1.0/(in_samplerate*FIR_PHASES));
		a1 = (-2.0*ap + 1.0/(in_samplerate*FIR_PHASES))/(2.0*bp + 1.0/(in_samplerate*FIR_PHASES));
		b1 = ( 2.0*bp - 1.0/(in_samplerate*FIR_PHASES))/(2.0*bp + 1.0/(in_samplerate*FIR_PHASES));
		double x = 0, y = 0;

		for(int i=0; i<FIR_TAPS; i++) {
			for(int j=0; j<FIR_PHASES; j++) {
				int mi = i*FIR_PHASES + j+1;	// match indexing of Matlab script
				sincpos = (mi)-(((FIR_TAPS*FIR_PHASES)+1.0)/2.0); // offset by 0.5 so sincpos!=0 (causes NaN x/0)
				firlowpass = sin(2 * M_PI * cutoff_freq * sincpos / (in_samplerate*FIR_PHASES)) / (M_PI * sincpos);

				y = a0*firlowpass + a1*x + b1*y;// Find the combined impulse response
				x = firlowpass;			// of FIR low-pass and IIR pre-emphasis
				firpreemph = y;			// y could be replaced by firpreemph but this
								// matches the example in the reference material

				window = (.54 - .46 * cos(2 * M_PI * mi / (double) FIR_TAPS*FIR_PHASES)); // Hamming window
				low_pass_fir[j][i] = firpreemph * window * gain;
			}
		}
#endif

		printf("Created low-pass FIR filter for audio channels, with cutoff at %.1i Hz\n", cutoff_freq);

		audio_pos = downsample_factor;
		audio_buffer = alloc_empty_buffer(length * channels);
		if(audio_buffer == NULL) return -1;
	}
	else {
		inf = NULL;
	}

	return 0;
}

int fm_mpx_get_samples(float *mpx_buffer) {
	if(rds) get_rds_samples(rds_buffer, length);

	if (inf == NULL) {
		for (int i=0; i<length; i++) {
			rds_buffer[i] *= carrier_57[phase_57++] * 0.05 * level_57;
			if (phase_57 == 4) phase_57 = 0;
		}
		memcpy(mpx_buffer, rds_buffer, buffer_size);
		return 0;
	}

	for(int i=0; i<length; i++) {
		if(audio_pos >= downsample_factor) {
			audio_pos -= downsample_factor;

			if(audio_len <= channels) {
				for(int j=0; j<2; j++) { // one retry
					audio_len = sf_read_float(inf, audio_buffer, length);
					if (audio_len < 0) {
						fprintf(stderr, "Error reading audio\n");
						return -1;
					} else if (audio_len == 0) {
						if( sf_seek(inf, 0, SEEK_SET) < 0 ) return 0;
					} else {
						break;
					}
				}
				audio_index = 0;
			} else {
				audio_index += channels;
				audio_len -= channels;
			}

#ifndef OLD_FILTER
			// First store the current sample(s) into the FIR filter's ring buffer
			fir_buffer_left[fir_index] = audio_buffer[audio_index];
			if(channels == 2) {
				fir_buffer_right[fir_index] = audio_buffer[audio_index+1];
			}
			fir_index++; // fir_index will point to newest valid data soon
			if(fir_index == FIR_TAPS) fir_index = 0;
#endif
		} // if need new sample

#ifdef OLD_FILTER
		fir_buffer_left[fir_index] = audio_buffer[audio_index];
		if (channels == 2) {
			fir_buffer_right[fir_index] = audio_buffer[audio_index+1];
		}
		fir_index++;
		if(fir_index == FIR_SIZE) fir_index = 0;
#endif

		// L/R signals
		float out_left = 0;
		float out_right = 0;

#ifdef OLD_FILTER
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
#else
		// Polyphase FIR filter

		// Calculate which FIR phase to use
		int iphase = ((int) (audio_pos*FIR_PHASES/downsample_factor)); // I think this is correct

		// use bit masking to implement circular buffer
		for(int fi=0; fi<FIR_TAPS; fi++) { // fi = Filter Index
			out_left += low_pass_fir[iphase][fi] * fir_buffer_left[(fir_index-fi)&(FIR_TAPS-1)];
			if(channels == 2) {
				out_right += low_pass_fir[iphase][fi] * fir_buffer_right[(fir_index-fi)&(FIR_TAPS-1)];
			}
		}
#endif

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

		mpx_buffer[i] +=
		carrier_57[phase_57++] * rds_buffer[i] * 0.05 * level_57;

		if (phase_57 == 4) phase_57 = 0;

		audio_pos++;
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
	rds = (new_level == 0) ? 0 : 1;
	level_57 = (new_level / 100.0);
}

int fm_mpx_close() {
	if(sf_close(inf)) fprintf(stderr, "Error closing audio file\n");

	if(audio_buffer != NULL) free(audio_buffer);

	return 0;
}
