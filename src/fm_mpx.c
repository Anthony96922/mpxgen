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

#define FIR_PHASES	32
#define FIR_TAPS	32 // MUST be a power of 2 for the circular buffer

size_t length;

// coefficients of the low-pass FIR filter
float low_pass_fir[FIR_PHASES][FIR_TAPS];

float carrier_19[] = {0, 0.5, 0.8660254, 1, 0.8660254, 0.5, 0, -0.5, -0.8660254, -1, -0.8660254, -0.5};
float carrier_38[] = {0, 0.8660254, 0.8660254, 0, -0.8660254, -0.8660254};
int phase_19 = 0;
int phase_38 = 0;

float downsample_factor;

float *audio_buffer;
int audio_index = 0;
int audio_len = 0;
float audio_pos;

float fir_buffer_left[FIR_TAPS] = {0};
float fir_buffer_right[FIR_TAPS] = {0};
int fir_index = 0;
int channels;

float rds_buffer[DATA_SIZE];
int rds;
int wait;

SNDFILE *inf;

float *alloc_empty_buffer(size_t length) {
    float *p = malloc(length * sizeof(float));
    if(p == NULL) return NULL;

    bzero(p, length * sizeof(float));

    return p;
}

int fm_mpx_open(char *filename, size_t len, int preemphasis, int rds_on, int wait_for_audio) {
	length = len;
	wait = wait_for_audio;
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
		if(channels > 1) {
			printf("%d channels, generating stereo multiplex.\n", channels);
		} else {
			printf("1 channel, monophonic operation.\n");
		}

		int cutoff_freq = 16000;
		if(in_samplerate/2 < cutoff_freq) cutoff_freq = in_samplerate/2;

		// Create the low-pass FIR filter, with pre-emphasis
		double window, firlowpass, firpreemph, sincpos;

		// IIR pre-emphasis filter
		// Reference material: http://jontio.zapto.org/hda1/preempiir.pdf
		double tau = preemphasis * 1e-6;
		double delta = 1.96e-6;
		double taup, deltap, bp, ap, a0, a1, b1;
		taup = 1.0/(2.0*(in_samplerate*FIR_PHASES))/tan(1.0/(2*tau*(in_samplerate*FIR_PHASES)));
		deltap = 1.0/(2.0*(in_samplerate*FIR_PHASES))/tan(1.0/(2*delta*(in_samplerate*FIR_PHASES)));
		bp = sqrt(-taup*taup + sqrt(taup*taup*taup*taup + 8.0*taup*taup*deltap*deltap)) / 2.0 ;
		ap = sqrt(2*bp*bp + taup*taup);
		a0 = ( 2.0*ap + 1/(in_samplerate*FIR_PHASES))/(2.0*bp + 1/(in_samplerate*FIR_PHASES));
		a1 = (-2.0*ap + 1/(in_samplerate*FIR_PHASES))/(2.0*bp + 1/(in_samplerate*FIR_PHASES));
		b1 = ( 2.0*bp + 1/(in_samplerate*FIR_PHASES))/(2.0*bp + 1/(in_samplerate*FIR_PHASES));
		double x = 0, y = 0;

		for(int i=0; i<FIR_TAPS; i++) {
			for(int j=0; j<FIR_PHASES; j++) {
				int mi = i*FIR_PHASES + j+1;	// match indexing of Matlab script
				sincpos = (mi)-(((FIR_TAPS*FIR_PHASES)+1.0)/2.0); // offset by 0.5 so sincpos!=0 (causes NaN x/0)
				firlowpass = sin(2 * M_PI * cutoff_freq * sincpos / (in_samplerate*FIR_PHASES) ) / (M_PI * sincpos);

				y = a0*firlowpass + a1*x + b1*y;// Find the combined impulse response
				x = firlowpass;			// of FIR low-pass and IIR pre-emphasis
				firpreemph = y;			// y could be replaced by firpreemph but this
								// matches the example in the reference material

				window = (.54 - .46 * cos(2*M_PI * (mi) / FIR_TAPS*FIR_PHASES)); // Hamming window
				low_pass_fir[j][i] = firpreemph * window * 9;
			}
		}

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

// samples provided by this function are in 0..10: they need to be divided by
// 10 after.
int fm_mpx_get_samples(float *mpx_buffer) {
	if(inf == NULL) {
		if(rds) get_rds_samples(mpx_buffer, length);
		return 0;
	} else {
		if(rds) get_rds_samples(rds_buffer, length);
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
					}
					if(audio_len == 0) {
						if( sf_seek(inf, 0, SEEK_SET) < 0 ) {
							if(wait) {
								return 0;
							} else {
								fprintf(stderr, "Could not rewind in audio file, terminating\n");
								return -1;
							}
						}
					} else {
						break;
					}
				}
				audio_index = 0;
			} else {
				audio_index += channels;
				audio_len -= channels;
			}

			fir_index++; // fir_index will point to newest valid data soon
			if(fir_index >= FIR_TAPS) fir_index = 0;
			// First store the current sample(s) into the FIR filter's ring buffer
			fir_buffer_left[fir_index] = audio_buffer[audio_index];
			if(channels > 1) fir_buffer_right[fir_index] = audio_buffer[audio_index+1];
		} // if need new sample

		// Polyphase FIR filter
		float out_left = 0;
		float out_right = 0;

		// Calculate which FIR phase to use
		int iphase = ((int) (audio_pos*FIR_PHASES/downsample_factor)); // I think this is correct
		int fi = 0;

		if( channels > 1 ) {
			for(fi=0; fi<FIR_TAPS; fi++)	// fi = Filter Index
			{				// use bit masking to implement circular buffer
				out_left  += low_pass_fir[iphase][fi]*fir_buffer_left[(fir_index-fi)&(FIR_TAPS-1)];
				out_right += low_pass_fir[iphase][fi]*fir_buffer_right[(fir_index-fi)&(FIR_TAPS-1)];
			}
		} else {
			for(fi=0; fi<FIR_TAPS; fi++)	// fi = Filter Index
			{				// use bit masking to implement circular buffer
				out_left += low_pass_fir[iphase][fi]*fir_buffer_left[(fir_index-fi)&(FIR_TAPS-1)];
			}
		}

		if (channels > 1) {
			mpx_buffer[i] +=
			10 * (out_left + out_right) +
			0.8 * carrier_19[phase_19] +
			10 * carrier_38[phase_38] * (out_left - out_right);

			phase_19++;
			phase_38++;
			if(phase_19 >= 12) phase_19 = 0;
			if(phase_38 >= 6) phase_38 = 0;
		} else
			mpx_buffer[i] = 10 * out_left;

		if (rds) mpx_buffer[i] += rds_buffer[i];

		audio_pos++;
	}

	return 0;
}


int fm_mpx_close() {
	if(sf_close(inf)) fprintf(stderr, "Error closing audio file\n");

	if(audio_buffer != NULL) free(audio_buffer);

	return 0;
}
