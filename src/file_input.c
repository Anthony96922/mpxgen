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
#include <sndfile.h>
#include "audio_conversion.h"

#define shortf_memcpy(x, y, z) memcpy(x, y, z * 2 * sizeof(short))

static uint8_t channels;
static uint8_t audio_wait;
static SNDFILE *inf;
static short *buf;
static size_t target_len;

int8_t open_file_input(char *filename, uint32_t *sample_rate, uint8_t wait, size_t num_frames) {
	// Open the input file
	SF_INFO sfinfo;

	target_len = num_frames;

	// stdin or file on the filesystem?
	if(filename[0] == '-' && filename[1] == 0) {
		if(!(inf = sf_open_fd(fileno(stdin), SFM_READ, &sfinfo, 0))) {
			fprintf(stderr, "Error: could not open stdin for audio input.\n");
			return -1;
		} else {
			fprintf(stderr, "Using stdin for audio input.\n");
		}
	} else {
		if(!(inf = sf_open(filename, SFM_READ, &sfinfo))) {
			fprintf(stderr, "Error: could not open input file %s.\n", filename);
			return -1;
		} else {
			fprintf(stderr, "Using audio file: %s\n", filename);
		}
	}

	*sample_rate = sfinfo.samplerate;
	channels = sfinfo.channels;
	audio_wait = wait;

	buf = malloc(num_frames * 2 * sizeof(short));

	return 0;
}

int16_t read_file_input(short *audio) {
	uint16_t read_len;
	uint16_t frames_to_read = target_len;
	uint16_t audio_len = 0;
	static uint8_t silent;

	while (frames_to_read > 0 && audio_len < target_len) {
		if ((read_len = sf_readf_short(inf, buf + (audio_len * channels), frames_to_read)) < 0) {
			fprintf(stderr, "Error reading audio\n");
			return -1;
		}

		audio_len += read_len;
		frames_to_read -= read_len;
		if (audio_len == 0) {
			// Check if we have more audio
			if (sf_seek(inf, 0, SEEK_SET) < 0) {
				if (audio_wait) {
					if (silent) {
						memset(buf, 0, target_len * 2 * sizeof(short));
					} else {
						silent = 1;
					}
					frames_to_read = 0;
				} else {
					return -1;
				}
			} else {
				silent = 0;
			}
		}
	}

	if (channels == 1)
		stereoizes16(buf, audio, target_len);
	else
		shortf_memcpy(audio, buf, target_len);

	return 1;
}

void close_file_input() {
	if (buf != NULL) free(buf);
	if (sf_close(inf)) fprintf(stderr, "Error closing audio file\n");
}
