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

#include <sndfile.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "rds.h"
#include "fm_mpx.h"
#include "mpx_carriers.h"

#define FIR_HALF_SIZE	30
#define FIR_SIZE	(2*FIR_HALF_SIZE-1)

// coefficients of the low-pass FIR filter
float low_pass_fir[FIR_HALF_SIZE];
float fir_buffer_left[FIR_SIZE];
float fir_buffer_right[FIR_SIZE];

float *audio_input;
float *resampled_input;

int channels;
int audio_wait;
int stop_audio;

SNDFILE *inf;

// SRC
int src_errorcode;

SRC_STATE *src_state;
SRC_DATA src_data;

int fm_mpx_open(char *filename, int wait_for_audio, int exit_on_audio_end) {
	audio_wait = wait_for_audio;
	stop_audio = exit_on_audio_end;

	if(filename == NULL) return 0;

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
	float upsample_factor = 228000. / in_samplerate;
	channels = sfinfo.channels;

	printf("Input: %d Hz, %d channels, upsampling factor: %.2f\n", in_samplerate, channels, upsample_factor);

	int cutoff_freq = 15500;
	if(cutoff_freq > in_samplerate/2) cutoff_freq = in_samplerate/2;

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

	audio_input = malloc(INPUT_DATA_SIZE * channels * sizeof(float));
	resampled_input = malloc(DATA_SIZE * channels * sizeof(float));
	if(audio_input == NULL) goto error;
	if(resampled_input == NULL) goto error;

	src_data.src_ratio = upsample_factor;
	src_data.output_frames = DATA_SIZE;
	src_data.data_in = audio_input;
	src_data.data_out = resampled_input;

	if ((src_state = src_new(CONVERTER_TYPE, channels, &src_errorcode)) == NULL) {
		fprintf(stderr, "Error: src_new failed: %s\n", src_strerror(src_errorcode));
		goto error;
	}

	return 0;

error:
	fm_mpx_close();
	return -1;
}

int get_input_audio() {
	int audio_len;

get_audio:
	audio_len = sf_readf_float(inf, audio_input, INPUT_DATA_SIZE);

	if (audio_len < 0) {
		fprintf(stderr, "Error reading audio\n");
		return -1;
	} else if (audio_len == 0) {
		if (stop_audio) return -1;
		// Check if we have more audio
		if( sf_seek(inf, 0, SEEK_SET) < 0 ) {
			if (audio_wait) {
				memset(resampled_input, 0, INPUT_DATA_SIZE * channels * sizeof(float));
				audio_len = INPUT_DATA_SIZE;
			} else {
				fprintf(stderr, "Could not rewind in audio file, terminating\n");
				return -1;
			}
		} else goto get_audio; // Try to get new audio
	} else { // Upsample the input
		src_data.input_frames = audio_len;
		if ((src_errorcode = src_process(src_state, &src_data))) {
			fprintf(stderr, "Error: src_process failed: %s\n", src_strerror(src_errorcode));
			return -1;
		}
		audio_len = src_data.output_frames_gen;
	}

	return audio_len;
}

float mpx_vol;

void set_output_volume(int vol) {
	mpx_vol = (vol / 100.0);
}

int fm_mpx_get_samples(float *mpx_buffer) {
	if (inf == NULL) {
		for (int i = 0; i < INPUT_DATA_SIZE; i++) {
			mpx_buffer[i] = get_57k_carrier() * get_rds_sample() * 0.08;
			update_carrier_phase();
			mpx_buffer[i] *= mpx_vol;
		}
		return INPUT_DATA_SIZE;
	}

	int buf_size = get_input_audio();

	static int fir_index;
	int j = 0;

	for (int i = 0; i < buf_size; i++) {
		// First store the current sample(s) into the FIR filter's ring buffer
		fir_buffer_left[fir_index] = resampled_input[j];
		if (channels == 2) {
			fir_buffer_right[fir_index] = resampled_input[j+1];
			j += 2;
		} else j++;
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

		// 6dB input gain
		out_left *= 2;
		out_right *= 2;

		// Create sum and difference signals
		float out_mono   = out_left + out_right;
		float out_stereo = out_left - out_right;

		if (channels == 2) {
			// audio signals need to be limited to 45% to remain within modulation limits
			mpx_buffer[i] = out_mono * 0.45 +
				get_19k_carrier() * 0.08 +
				get_38k_carrier() * out_stereo * 0.45;
		} else {
			// mono audio is limited to 90%
			mpx_buffer[i] = out_mono * 0.9;
		}

		mpx_buffer[i] += get_57k_carrier() * get_rds_sample() * 0.12;

		update_carrier_phase();

		mpx_buffer[i] *= mpx_vol;
	}

	return buf_size;
}

void fm_mpx_close() {
	if(sf_close(inf)) fprintf(stderr, "Error closing audio file\n");

	if(audio_input != NULL) free(audio_input);
	if(resampled_input != NULL) free(resampled_input);
	src_delete(src_state);
}
