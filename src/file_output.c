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
#include "file_output.h"

static SNDFILE *inf;

int open_file_output(char *filename, unsigned int sample_rate, unsigned int channels) {
        SF_INFO sfinfo;
	sfinfo.samplerate = sample_rate;
	sfinfo.channels = channels;
	sfinfo.format = SF_FORMAT_PCM_16;

	// stdout or file on the filesystem?
	if(filename[0] == '-' && filename[1] == 0) {
		sfinfo.format |= SF_FORMAT_RAW;
		if(!(inf = sf_open_fd(fileno(stdout), SFM_WRITE, &sfinfo, 0))) {
			fprintf(stderr, "Error: could not open stdout for audio output.\n");
			return -1;
		} else {
			fprintf(stderr, "Using stdout for audio output.\n");
		}
	} else {
		sfinfo.format |= SF_FORMAT_WAV;
		if(!(inf = sf_open(filename, SFM_WRITE, &sfinfo))) {
			fprintf(stderr, "Error: could not open output file %s.\n", filename);
			return -1;
		} else {
			fprintf(stderr, "Using audio file: %s\n", filename);
		}
	}

	return 0;
}

int write_file_output(short *audio, size_t num_frames) {
	int audio_len;

	if ((audio_len = sf_writef_short(inf, audio, num_frames)) < 0) {
		return -1;
	}

	return 1;
}

void close_file_output() {
	if (sf_close(inf)) fprintf(stderr, "Error closing audio file\n");
}
