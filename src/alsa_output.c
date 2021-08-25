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

static snd_pcm_t *pcm;

int8_t open_alsa_output(char *output_device, unsigned int sample_rate, unsigned int channels) {
	int8_t err;
#if 0
	snd_pcm_hw_params_t *hw_params;

	err = snd_pcm_hw_params_malloc(&hw_params);
	if (err < 0) {
		fprintf(stderr, "Error: cannot allocate hardware parameter structure (%s)\n", snd_strerror(err));
		return -1;
	}
#endif

	err = snd_pcm_open(&pcm, output_device, SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0) {
		fprintf(stderr, "Error: cannot open output audio device '%s' (%s)\n", output_device, snd_strerror(err));
		return -1;
	}

#if 0
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
		fprintf(stderr, "Error: cannot set sample rate to %u Hz: %s\n", sample_rate, snd_strerror(err));
		return -1;
	}

	err = snd_pcm_hw_params_set_channels(pcm, hw_params, channels);
	if (err < 0) {
		fprintf(stderr, "Error: cannot set channel count to %u (%s)\n", channels, snd_strerror(err));
		return -1;
	}

	/*
	err = snd_pcm_hw_params_set_buffer_size(pcm, hw_params, buffer_size);
	if (err < 0) {
		fprintf(stderr, "Error: cannot set buffer size to %u (%s)\n", buffer_size, snd_strerror(err));
		return -1;
	}
	*/

	err = snd_pcm_hw_params(pcm, hw_params);
	if (err < 0) {
		fprintf(stderr, "Error: unable to set hw params for playback (%s)\n", snd_strerror(err));
		return -1;
	}

	snd_pcm_hw_params_free(hw_params);
#else
	err = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE,
		SND_PCM_ACCESS_RW_INTERLEAVED,
		channels, sample_rate,
		0,
		50000);
	if (err < 0) {
		fprintf(stderr, "Cannot open open output device (%s)\n", snd_strerror(err));
		return -1;
	}
#endif

#if 0
	err = snd_pcm_prepare(pcm);
	if (err < 0) {
		fprintf(stderr, "Error: cannot prepare audio interface for use (%s)\n", snd_strerror(err));
		return -1;
	}
#endif

	return 0;
}

int16_t write_alsa_output(short *buffer, size_t frames) {
	int frames_written;

	frames_written = snd_pcm_writei(pcm, buffer, frames);

	if (frames_written < 0) {
		frames_written = snd_pcm_recover(pcm, frames_written, 0);
	}

	if (frames_written < 0) {
		fprintf(stderr, "Error: write to audio device failed (%s)\n", snd_strerror(frames_written));
		return -1;
	}

	return frames_written;
}

int8_t close_alsa_output() {
	int err;

	err = snd_pcm_drain(pcm);
	if (err < 0) {
		fprintf(stderr, "Error: could not drain sink (%s)\n", snd_strerror(err));
		return -1;
	}
	snd_pcm_close(pcm);

	return 0;
}
