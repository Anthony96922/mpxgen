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
#include "input.h"

int input_type;

int open_input(char *input_name, int wait, unsigned int *sample_rate) {
#ifdef ALSA
	// TODO: better detect live capture cards
	if (strstr(input_name, ":") != NULL) {
		*sample_rate = 48000;
		if (open_alsa_input(input_name, *sample_rate, NUM_AUDIO_FRAMES_IN) < 0) {
			fprintf(stderr, "Could not open ALSA source.\n");
			return 0;
		}
		input_type = 2;
	} else {
#endif
		if (open_file_input(input_name, sample_rate, wait, NUM_AUDIO_FRAMES_IN) < 0) {
			return 0;
		}
		input_type = 1;
#ifdef ALSA
	}
#endif

	if (*sample_rate < 16000) {
		fprintf(stderr, "Input sample rate must be at least 16k.\n");
		return -1;
        }

	return 1;
}

int get_input(float *audio) {
	switch (input_type) {
	case 1:
		if (read_file_input(audio) < 0) return -1;
		break;
#ifdef ALSA
	case 2:
		if (read_alsa_input(audio) < 0) return -1;
		break;
#endif
	}

	return 0;
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
}
