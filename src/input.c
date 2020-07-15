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
#include <string.h>
#include "input.h"

int channels;
int audio_wait;

SNDFILE *open_file_input(char *filename, int *input_sample_rate, int *input_channels, int wait) {
	// Open the input file
        SF_INFO sfinfo;
	SNDFILE *inf;

	// stdin or file on the filesystem?
	if(strcmp(filename, "-") == 0) {
		if(!(inf = sf_open_fd(fileno(stdin), SFM_READ, &sfinfo, 0))) {
			fprintf(stderr, "Error: could not open stdin for audio input.\n");
			return NULL;
		} else {
			fprintf(stderr, "Using stdin for audio input.\n");
		}
	} else {
		if(!(inf = sf_open(filename, SFM_READ, &sfinfo))) {
			fprintf(stderr, "Error: could not open input file %s.\n", filename);
			return NULL;
		} else {
			fprintf(stderr, "Using audio file: %s\n", filename);
		}
	}

	*input_sample_rate = sfinfo.samplerate;
        *input_channels = sfinfo.channels;
	channels = sfinfo.channels;
	audio_wait = wait;

	return inf;
}

int read_file_input(SNDFILE *inf, float *audio, size_t num_frames) {
	int audio_len;
	int frames_to_read = num_frames;
	int buffer_offset = 0;

	while (frames_to_read) {
		audio_len = sf_readf_float(inf, audio + buffer_offset, frames_to_read);

		if (audio_len < 0) {
			fprintf(stderr, "Error reading audio\n");
			return -1;
		} else {
			buffer_offset += audio_len;
			frames_to_read -= audio_len;
			// Check if we have more audio
			if (audio_len == 0) {
				if (sf_seek(inf, 0, SEEK_SET) < 0) {
					if (audio_wait) {
						memset(audio, 0, num_frames * channels * sizeof(float));
						frames_to_read = 0;
					} else {
						fprintf(stderr, "Could not rewind in audio file, terminating\n");
						return -1;
					}
				}
			}
		}
	}

	return 1;
}

void close_file_input(SNDFILE *inf) {
	if (sf_close(inf)) fprintf(stderr, "Error closing audio file\n");
}
