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
#include <alsa/asoundlib.h>
#include "audio_conversion.h"

static short *short_buffer;
static size_t buffer_size;

snd_pcm_t *pcm;

int open_alsa_input(char *input, unsigned int sample_rate, size_t num_frames) {
	int err;
	snd_pcm_hw_params_t *hw_params;

	buffer_size = num_frames;
	short_buffer = malloc(buffer_size * 2 * sizeof(short));

	if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		fprintf(stderr, "Error: cannot allocate hardware parameter structure (%s)\n", snd_strerror(err));
		return -1;
	}

	if ((err = snd_pcm_open(&pcm, input, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		fprintf(stderr, "Error: cannot open input audio device '%s' (%s)\n", input, snd_strerror(err));
		return -1;
	}

	if ((err = snd_pcm_hw_params_any(pcm, hw_params)) < 0) {
		fprintf(stderr, "Error: cannot initialize hardware parameter structure (%s)\n", snd_strerror(err));
		return -1;
	}

	if ((err = snd_pcm_hw_params_set_access(pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf(stderr, "Error: cannot set access type (%s)\n", snd_strerror(err));
		return -1;
	}

	if ((err = snd_pcm_hw_params_set_format(pcm, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
		fprintf(stderr, "Error: cannot set sample format (%s)\n", snd_strerror(err));
		return -1;
	}

	if ((err = snd_pcm_hw_params_set_rate(pcm, hw_params, sample_rate, 0)) < 0) {
		fprintf(stderr, "Error: cannot set sample rate (%s)\n", snd_strerror(err));
		return -1;
	}

	if ((err = snd_pcm_hw_params_set_channels(pcm, hw_params, 2)) < 0) {
		fprintf(stderr, "Error: cannot set channel count (%s)\n", snd_strerror(err));
		return -1;
	}

	if ((err = snd_pcm_hw_params(pcm, hw_params)) < 0) {
		fprintf(stderr, "Error: cannot set parameters (%s)\n", snd_strerror(err));
		return -1;
	}

	snd_pcm_hw_params_free(hw_params);

	if ((err = snd_pcm_prepare(pcm)) < 0) {
		fprintf(stderr, "Error: cannot prepare audio interface for use (%s)\n", snd_strerror(err));
		return -1;
	}

	fprintf(stderr, "Using ADC input %s.\n", input);

	return 0;
}

int read_alsa_input(float *buffer) {
	int frames_read;

	if ((frames_read = snd_pcm_readi(pcm, short_buffer, buffer_size)) < 0) {
		fprintf(stderr, "Error: read from audio interface failed (%s)\n", snd_strerror(frames_read));
	} else {
		short2float(short_buffer, buffer, buffer_size);
	}

	return frames_read;
}

int close_alsa_input() {
	int err;

	if ((err = snd_pcm_close(pcm)) < 0) {
		fprintf(stderr, "Error: could not close source (%s)\n", snd_strerror(err));
	}

	free(short_buffer);

	return err;
}
