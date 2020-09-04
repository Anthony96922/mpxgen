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
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "rds.h"
#ifdef RDS2
#include "rds2.h"
#endif
#include "fm_mpx.h"
#include "mpx_carriers.h"
#include "resampler.h"
#include "input.h"

#define FIR_HALF_SIZE	30
#define FIR_SIZE	(2*FIR_HALF_SIZE-1)

// coefficients of the low-pass FIR filter
float low_pass_fir[FIR_HALF_SIZE];
float fir_buffer[2][FIR_SIZE];

float *input_buffer;
float *mpx_buffer;

// SRC
SRC_STATE *mpx_resampler;
SRC_DATA mpx_resampler_data;

int channels;
int input;

float mpx_vol;

void set_output_volume(int vol) {
	mpx_vol = (vol / 100.0);
}

int fm_mpx_open(char *filename, int wait_for_audio, float out_ppm) {
	int cutoff_freq;

	mpx_buffer = malloc(DATA_SIZE * sizeof(float));

	mpx_resampler_data.src_ratio = (192000 / (double)190000) + (out_ppm / 1e6);
	mpx_resampler_data.output_frames = DATA_SIZE;
	mpx_resampler_data.data_in = mpx_buffer;

	if ((mpx_resampler = resampler_init(1)) == NULL) {
		fprintf(stderr, "Could not create MPX resampler.\n");
		goto error;
	}

	create_mpx_carriers(190000);

	if (filename != NULL) {
		if (!(channels = open_input(filename, wait_for_audio))) goto error;
	} else {
		return 0;
	}

	input = 1;
	cutoff_freq = 24000;

	input_buffer = malloc(DATA_SIZE * sizeof(float));

	// Here we divide this coefficient by two because it will be counted twice
	// when applying the filter
	low_pass_fir[FIR_HALF_SIZE-1] = 2 * cutoff_freq / 190000 / 2;

	// Only store half of the filter since it is symmetric
	for(int i=1; i<FIR_HALF_SIZE; i++) {
		low_pass_fir[FIR_HALF_SIZE-1-i] =
			sin(2 * M_PI * cutoff_freq * i / 190000) / (M_PI * i) // sinc
			* (.54 - .46 * cos(2 * M_PI * (i+FIR_HALF_SIZE) / (2*FIR_HALF_SIZE))); // Hamming window
	}

	fprintf(stderr, "Created low-pass FIR filter for audio channels, with cutoff at %d Hz\n", cutoff_freq);

	return 0;

error:
	fm_mpx_close();
	return -1;
}

int fm_mpx_get_samples(float *out) {
	int j = 0;
	int audio_len;
	static int fir_index;

	int ifbi, dfbi;
	float out_left, out_right;
	float out_mono, out_stereo;

	if (!input) {
		audio_len = INPUT_DATA_SIZE;

		for (int i = 0; i < audio_len; i++) {
			// 6% modulation
			mpx_buffer[i] = get_carrier(2) * get_rds_sample() * 0.12;

#ifdef RDS2
			mpx_buffer[i] += get_carrier(3) * get_rds2_sample(1) * 0.12;
			mpx_buffer[i] += get_carrier(4) * get_rds2_sample(2) * 0.12;
			mpx_buffer[i] += get_carrier(5) * get_rds2_sample(3) * 0.12;
#endif

			update_carrier_phase();

			mpx_buffer[i] *= mpx_vol;
		}
	} else {
		if ((audio_len = read_input(input_buffer)) < 0) return -1;

		for (int i = 0; i < audio_len; i++) {
			// First store the current sample(s) into the FIR filter's ring buffer
			fir_buffer[0][fir_index] = input_buffer[j];
			if (channels == 2) {
				fir_buffer[1][fir_index] = input_buffer[j+1];
				j++;
			}
			j++;
			fir_index++;
			if(fir_index == FIR_SIZE) fir_index = 0;

			// L/R signals
			out_left  = 0;
			out_right = 0;

			// Now apply the FIR low-pass filter

			/* As the FIR filter is symmetric, we do not multiply all
			   the coefficients independently, but two-by-two, thus reducing
			   the total number of multiplications by a factor of two
			 */
			ifbi = fir_index;  // ifbi = increasing FIR Buffer Index
			dfbi = fir_index;  // dfbi = decreasing FIR Buffer Index
			for(int fi=0; fi<FIR_HALF_SIZE; fi++) {  // fi = Filter Index
				dfbi--;
				if(dfbi < 0) dfbi = FIR_SIZE-1;
				out_left += low_pass_fir[fi] * (fir_buffer[0][ifbi] + fir_buffer[0][dfbi]);
				if(channels == 2) {
					out_right += low_pass_fir[fi] * (fir_buffer[1][ifbi] + fir_buffer[1][dfbi]);
				}
				ifbi++;
				if(ifbi == FIR_SIZE) ifbi = 0;
			}
			// End of FIR filter

			// 6dB input gain
			out_left *= 2;
			out_right *= 2;

			// Create sum and difference signals
			out_mono   = out_left + out_right;
			out_stereo = out_left - out_right;

			if (channels == 2) {
				// audio signals need to be limited to 45% to remain within modulation limits
#if 0
				// Polar stereo encoding system used in Eastern Europe
				mpx_buffer[i] = out_mono * 0.45 +
					get_carrier(3) * ((out_stereo * 0.45) + 0.08);
#else
				mpx_buffer[i] = out_mono * 0.45 +
					get_carrier(0) * 0.08 + // 8% modulation
					get_carrier(1) * out_stereo * 0.45;
#endif
			} else {
				// mono audio is limited to 90%
				mpx_buffer[i] = out_mono * 0.9;
			}

			// 6% modulation
			mpx_buffer[i] += get_carrier(2) * get_rds_sample() * 0.12;

#ifdef RDS2
			mpx_buffer[i] += get_carrier(3) * get_rds2_sample(1) * 0.12;
			mpx_buffer[i] += get_carrier(4) * get_rds2_sample(2) * 0.12;
			mpx_buffer[i] += get_carrier(5) * get_rds2_sample(3) * 0.12;
#endif

			update_carrier_phase();

			mpx_buffer[i] *= mpx_vol;
		}
	}

	mpx_resampler_data.input_frames = audio_len;
	mpx_resampler_data.data_out = out;
	if ((audio_len = resample(mpx_resampler, mpx_resampler_data)) < 0) return -1;

	return audio_len;
}

void fm_mpx_close() {
	close_input();
	clear_mpx_carriers();
	if (mpx_buffer != NULL) free(mpx_buffer);
	resampler_exit(mpx_resampler);
}
