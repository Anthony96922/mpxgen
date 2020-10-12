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
#include "fm_mpx.h"
#include "resampler.h"
#include "input.h"

float *audio_input;

int input_type;

// SRC
SRC_STATE *input_resampler;
SRC_DATA input_resampler_data;

int open_input(char *input_name, int wait) {
	unsigned int sample_rate;
	unsigned int channels;
	float upsample_factor;

#ifdef ALSA
	// TODO: better detect live capture cards
	if (strstr(input_name, ":") != NULL) {
		channels = 2;
		sample_rate = 48000;
		if (open_alsa_input(input_name, sample_rate, channels, INPUT_DATA_SIZE) < 0) {
			fprintf(stderr, "Could not open ALSA source.\n");
			return 0;
		}
		input_type = 2;
	} else {
#endif
		if (open_file_input(input_name, &sample_rate, &channels, wait, INPUT_DATA_SIZE) < 0) {
			return 0;
		}
		input_type = 1;
#ifdef ALSA
	}
#endif

	if (sample_rate < 16000) {
		fprintf(stderr, "Input sample rate must be at least 16k.\n");
		return -1;
        }

	upsample_factor = (double)190000 / sample_rate;

	fprintf(stderr, "Input: %d Hz, %d channels, upsampling factor: %.2f\n", sample_rate, channels, upsample_factor);

	audio_input = malloc(INPUT_DATA_SIZE * channels * sizeof(float));

	input_resampler_data.src_ratio = upsample_factor;
	// output_frames: max number of frames to generate
	// Because we're upsampling the input, the number of output frames
	// needs to be at least the number of input_frames times the ratio.
	input_resampler_data.input_frames = INPUT_DATA_SIZE;
	input_resampler_data.output_frames = DATA_SIZE;
	input_resampler_data.data_in = audio_input;

	if ((input_resampler = resampler_init(channels)) == NULL) {
		fprintf(stderr, "Could not create input resampler.\n");
		return -1;
	}

	return channels;
}

int read_input(float *audio) {
	switch (input_type) {
	case 1:
		if (read_file_input(audio_input) < 0) return -1;
		break;
#ifdef ALSA
	case 2:
		if (read_alsa_input(audio_input) < 0) return -1;
		break;
#endif
	}

	input_resampler_data.data_out = audio;
	return resample(input_resampler, input_resampler_data);

	// TODO input buffering goes here (see mpx_gen.c)
}

void close_input() {
	switch (input_type) {
	case 1:
		close_file_input();
		break;
#ifdef ALSA
	case 2:
		close_alsa_input();
		break;
#endif
	}

	resampler_exit(input_resampler);
	free(audio_input);
}
