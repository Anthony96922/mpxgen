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
#include <alsa/asoundlib.h>

static size_t buffer_size;
static snd_pcm_t *pcm;

int8_t open_alsa_input(char *input, uint32_t sample_rate, size_t num_frames) {
	int err;
	snd_pcm_hw_params_t *hw_params;

	buffer_size = num_frames;

	err = snd_pcm_hw_params_malloc(&hw_params);
	if (err < 0) {
		fprintf(stderr, "Error: cannot allocate hardware parameter structure (%s)\n", snd_strerror(err));
		return -1;
	}

	err = snd_pcm_open(&pcm, input, SND_PCM_STREAM_CAPTURE, 0);
	if (err < 0) {
		fprintf(stderr, "Error: cannot open input audio device '%s' (%s)\n", input, snd_strerror(err));
		return -1;
	}

	err = snd_pcm_hw_params_any(pcm, hw_params);
	if (err < 0) {
		fprintf(stderr, "Error: no configurations available (%s)\n", snd_strerror(err));
		return -1;
	}

	err = snd_pcm_hw_params_set_access(pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		fprintf(stderr, "Error: cannot set access type (%s)\n", snd_strerror(err));
		return -1;
	}

	err = snd_pcm_hw_params_set_format(pcm, hw_params, SND_PCM_FORMAT_S16_LE);
	if (err < 0) {
		fprintf(stderr, "Error: cannot set sample format (%s)\n", snd_strerror(err));
		return -1;
	}

	err = snd_pcm_hw_params_set_rate(pcm, hw_params, sample_rate, 0);
	if (err < 0) {
		fprintf(stderr, "Error: cannot set sample rate (%s)\n", snd_strerror(err));
		return -1;
	}

	err = snd_pcm_hw_params_set_channels(pcm, hw_params, 2);
	if (err < 0) {
		fprintf(stderr, "Error: cannot set channel count (%s)\n", snd_strerror(err));
		return -1;
	}

	err = snd_pcm_hw_params(pcm, hw_params);
	if (err < 0) {
		fprintf(stderr, "Error: cannot set parameters (%s)\n", snd_strerror(err));
		return -1;
	}

	snd_pcm_hw_params_free(hw_params);

	err = snd_pcm_prepare(pcm);
	if (err < 0) {
		fprintf(stderr, "Error: cannot prepare audio interface for use (%s)\n", snd_strerror(err));
		return -1;
	}

	return 0;
}

int16_t read_alsa_input(short *buffer) {
	int16_t frames_read;
	uint16_t frames;

	frames_read = snd_pcm_readi(pcm, buffer, buffer_size);
	if (frames_read < 0) {
		fprintf(stderr, "Error: read from audio device failed (%s)\n", snd_strerror(frames_read));
		frames = -1;
	} else {
		frames = frames_read;
	}

	return frames;
}

int8_t close_alsa_input() {
	int8_t err;

	err = snd_pcm_close(pcm);
	if (err < 0) {
		fprintf(stderr, "Error: could not close source (%s)\n", snd_strerror(err));
	}

	return err;
}
