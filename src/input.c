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

#include "common.h"
#include "input.h"

static uint8_t input_type;

int8_t open_input(char *input_name, uint8_t wait, uint32_t *sample_rate, size_t num_frames) {
	// TODO: better detect live capture cards
	if (input_name[0] == 'a' && input_name[1] == 'l' &&
	    input_name[2] == 's' && input_name[3] == 'a' &&
	    input_name[4] == ':') { // check if name is prefixed with "alsa:"
		*sample_rate = 48000;
		input_name = input_name+5; // don't pass prefix
		if (open_alsa_input(input_name, *sample_rate, num_frames) < 0) {
			fprintf(stderr, "Could not open ALSA source.\n");
			return 0;
		}
		input_type = 2;
	} else {
		if (open_file_input(input_name, sample_rate, wait, num_frames) < 0) {
			return 0;
		}
		input_type = 1;
	}

	if (*sample_rate < 16000) {
		fprintf(stderr, "Input sample rate must be at least 16k.\n");
		return -1;
        }

	return 1;
}

int8_t read_input(short *audio) {
	if (input_type == 1) {
		if (read_file_input(audio) < 0) return -1;
	}
	if (input_type == 2) {
		if (read_alsa_input(audio) < 0) return -1;
	}
	return 0;
}

void close_input() {
	if (input_type == 1) {
		close_file_input();
	}
	if (input_type == 2) {
		close_alsa_input();
	}
}
