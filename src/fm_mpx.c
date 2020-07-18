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
#ifdef ALSA
#include "alsa_input.h"
#endif

#define FIR_HALF_SIZE	30
#define FIR_SIZE	(2*FIR_HALF_SIZE-1)

// coefficients of the low-pass FIR filter
float low_pass_fir[FIR_HALF_SIZE];
float fir_buffer_left[FIR_SIZE];
float fir_buffer_right[FIR_SIZE];

float *audio_input;
float *resampled_input;
float *mpx_buffer;
float *mpx_out;

#ifdef ALSA
snd_pcm_t *alsa_input;
#endif

int channels;

int input;

SNDFILE *inf;

// SRC
SRC_STATE *in_resampler;
SRC_DATA in_resampler_data;
SRC_STATE *mpx_resampler;
SRC_DATA mpx_resampler_data;

float mpx_vol;

void set_output_volume(int vol) {
	mpx_vol = (vol / 100.0);
}

int fm_mpx_open(char *filename, int wait_for_audio, float out_ppm) {
	int in_samplerate;
	float upsample_factor;
	int cutoff_freq = 15500;

	mpx_buffer = malloc(DATA_SIZE * sizeof(float));
	mpx_out = malloc(DATA_SIZE * sizeof(float));

	mpx_resampler_data.src_ratio = (192000 / 228000.0) + (out_ppm / 1000000);
	mpx_resampler_data.output_frames = DATA_SIZE;
	mpx_resampler_data.data_in = mpx_buffer;
	mpx_resampler_data.data_out = mpx_out;

	if ((mpx_resampler = resampler_init(1)) == NULL) {
		fprintf(stderr, "Could not create MPX resampler.\n");
		goto error;
	}

	create_mpx_carriers(228000);

#ifdef ALSA
	char *input_card;
#endif

	if(filename != NULL) {
		if ((inf = open_file_input(filename, &in_samplerate, &channels, wait_for_audio, INPUT_DATA_SIZE)) == NULL)
			goto error;
		input = 1;
#ifdef ALSA
	} else if (input_card != NULL) {
		channels = 1;
		in_samplerate = 48000;
		if ((alsa_input = open_alsa_input(input_card, in_samplerate, channels, INPUT_DATA_SIZE)) == NULL)
			goto error;
		input = 2;
#endif
	} else {
		return 0;
	}

	upsample_factor = 228000. / in_samplerate;

	fprintf(stderr, "Input: %d Hz, %d channels, upsampling factor: %.2f\n", in_samplerate, channels, upsample_factor);

	// Here we divide this coefficient by two because it will be counted twice
	// when applying the filter
	low_pass_fir[FIR_HALF_SIZE-1] = 2 * cutoff_freq / 228000 /2;

	// Only store half of the filter since it is symmetric
	for(int i=1; i<FIR_HALF_SIZE; i++) {
		low_pass_fir[FIR_HALF_SIZE-1-i] =
			sin(2 * M_PI * cutoff_freq * i / 228000) / (M_PI * i) // sinc
			* (.54 - .46 * cos(2 * M_PI * (i+FIR_HALF_SIZE) / (2*FIR_HALF_SIZE))); // Hamming window
	}

	fprintf(stderr, "Created low-pass FIR filter for audio channels, with cutoff at %d Hz\n", cutoff_freq);

	audio_input = malloc(DATA_SIZE * channels * sizeof(float));
	resampled_input = malloc(DATA_SIZE * channels * sizeof(float));

	in_resampler_data.src_ratio = upsample_factor;
	in_resampler_data.input_frames = INPUT_DATA_SIZE;
	in_resampler_data.output_frames = DATA_SIZE;
	in_resampler_data.data_in = audio_input;
	in_resampler_data.data_out = resampled_input;

	if ((in_resampler = resampler_init(channels)) == NULL) {
		fprintf(stderr, "Could not create input resampler.\n");
		goto error;
	}

	return 0;

error:
	fm_mpx_close();
	return -1;
}

int get_input_audio() {
	if (input == 1) {
		if (read_file_input(inf, audio_input) < 0) return -1;
#ifdef ALSA
	} else if (input == 2) {
		if (read_alsa_input(alsa_input, audio_input) < 0) return -1;
#endif
	}
	return resample(in_resampler, in_resampler_data);
}

int fm_mpx_get_samples(float *out) {
	int j = 0;
	int audio_len;
	static int fir_index;

	int ifbi, dfbi;
	float out_left, out_right;
	float out_mono, out_stereo;

	if (!input) {
		audio_len = DATA_SIZE;

		for (int i = 0; i < audio_len; i++) {
			// 6% modulation
			mpx_buffer[i] = get_57k_carrier() * get_rds_sample() * 0.12;

#ifdef RDS2
			mpx_buffer[i] += get_67k_carrier() * get_rds2_stream1_sample() * 0.12;
			mpx_buffer[i] += get_71k_carrier() * get_rds2_stream2_sample() * 0.12;
			mpx_buffer[i] += get_76k_carrier() * get_rds2_stream3_sample() * 0.12;
#endif

			update_carrier_phase();

			mpx_buffer[i] *= mpx_vol;
		}
	} else {
		if ((audio_len = get_input_audio()) < 0) return -1;

		for (int i = 0; i < audio_len; i++) {
			// First store the current sample(s) into the FIR filter's ring buffer
			fir_buffer_left[fir_index] = resampled_input[j];
			if (channels == 2) {
				fir_buffer_right[fir_index] = resampled_input[j+1];
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
			out_mono   = out_left + out_right;
			out_stereo = out_left - out_right;

			if (channels == 2) {
				// audio signals need to be limited to 45% to remain within modulation limits
				mpx_buffer[i] = out_mono * 0.45 +
					get_19k_carrier() * 0.08 + // 8% modulation
					get_38k_carrier() * out_stereo * 0.45;
			} else {
				// mono audio is limited to 90%
				mpx_buffer[i] = out_mono * 0.9;
			}

			// 6% modulation
			mpx_buffer[i] += get_57k_carrier() * get_rds_sample() * 0.12;

#ifdef RDS2
			mpx_buffer[i] += get_67k_carrier() * get_rds2_stream1_sample() * 0.12;
			mpx_buffer[i] += get_71k_carrier() * get_rds2_stream2_sample() * 0.12;
			mpx_buffer[i] += get_76k_carrier() * get_rds2_stream3_sample() * 0.12;
#endif

			update_carrier_phase();

			mpx_buffer[i] *= mpx_vol;
		}
	}

	mpx_resampler_data.input_frames = audio_len;
	if ((audio_len = resample(mpx_resampler, mpx_resampler_data)) < 0) return -1;

	memcpy(out, mpx_out, audio_len * sizeof(float));

	return audio_len;
}

void fm_mpx_close() {
	if (input == 1) {
		close_file_input(inf);
#ifdef ALSA
	} else if (input == 2) {
		close_alsa_input(alsa_input);
#endif
	}
	clear_mpx_carriers();
	if (audio_input != NULL) free(audio_input);
	if (resampled_input != NULL) free(resampled_input);
	if (mpx_buffer != NULL) free(mpx_buffer);
	if (mpx_out != NULL) free(mpx_out);
	resampler_exit(in_resampler);
	resampler_exit(mpx_resampler);
}
