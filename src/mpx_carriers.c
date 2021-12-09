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
#include "mpx_carriers.h"

/*
 * Code for MPX oscillator
 *
 * This uses lookup tables to speed up the waveform generation
 *
 */

/*
 * DDS function generator
 *
 * Create wave constants for a given frequency
 */
static void create_wave(uint32_t rate, float freq, float *sin_wave, float *cos_wave, uint16_t *max_phase) {
	float sin_sample, cos_sample;
	// used to determine if we have completed a cycle
	uint8_t zero_crossings = 0;
	uint16_t i;
	double w = M_2PI * freq;
	double phase;

	// First value of a sine wave is always 0
	*sin_wave++ = 0.0f;
	*cos_wave++ = 1.0f;

	for (i = 1; i < rate; i++) {
		phase = i / (double)rate;
		sin_sample = sin(w * phase);
		cos_sample = cos(w * phase);
		if (sin_sample > -0.1e-4 && sin_sample < 0.1e-4) {
			if (++zero_crossings == 2) break;
			*sin_wave++ = 0.0f;
		} else {
			*sin_wave++ = sin_sample;
		}
		*cos_wave++ = cos_sample;
	}

	*max_phase = i;
}

/*
 * Create waveform lookup tables for frequencies in the array
 *
 */
void init_osc(struct osc_t *osc_ctx, uint32_t sample_rate, const float *c_freqs) {
	uint8_t num_freqs = 0;
	// look for the 0 terminator
	for (;;) {
		if (c_freqs[num_freqs] == 0.0) break;
		num_freqs++;
	}

	osc_ctx->num_freqs = num_freqs;

	/*
	 * waveform tables
	 *
	 * first index is wave frequency
	 * second index is wave data
	 */
	osc_ctx->sine_waves = malloc(num_freqs * sizeof(float));
	osc_ctx->cosine_waves = malloc(num_freqs * sizeof(float));
	/*
	 * phase table
	 *
	 * current and max
	 */
	osc_ctx->phases = malloc(num_freqs * 2 * sizeof(uint16_t));

	for (uint8_t i = 0; i < num_freqs; i++) {
		osc_ctx->sine_waves[i] = malloc(sample_rate * sizeof(float));
		osc_ctx->cosine_waves[i] = malloc(sample_rate * sizeof(float));
		osc_ctx->phases[i] = malloc(2 * sizeof(uint16_t));
		osc_ctx->phases[i][CURRENT] = 0;

		// create waveform data and load into lookup tables
		create_wave(sample_rate, c_freqs[i],
			osc_ctx->sine_waves[i],
			osc_ctx->cosine_waves[i],
			&osc_ctx->phases[i][MAX]
		);
	}
}

/*
 * Get a waveform sample for a given frequency
 *
 * Cosine is needed for SSB generation
 *
 */
float get_wave(struct osc_t *osc_ctx, uint8_t waveform_num, uint8_t cosine) {
	uint16_t cur_phase = osc_ctx->phases[waveform_num][CURRENT];
	if (cosine) {
		return osc_ctx->cosine_waves[waveform_num][cur_phase];
	} else {
		return osc_ctx->sine_waves[waveform_num][cur_phase];
	}
}

/*
 * Shift the oscillator to the next phase
 *
 */
void update_osc_phase(struct osc_t *osc_ctx) {
	for (uint8_t i = 0; i < osc_ctx->num_freqs; i++) {
		if (++osc_ctx->phases[i][CURRENT] == osc_ctx->phases[i][MAX])
			osc_ctx->phases[i][CURRENT] = 0;
	}
}

/*
 * Unload all waveform and phase tables
 *
 */
void exit_osc(struct osc_t *osc_ctx) {
	for (uint8_t i = 0; i < osc_ctx->num_freqs; i++) {
		free(osc_ctx->sine_waves[i]);
		free(osc_ctx->cosine_waves[i]);
		free(osc_ctx->phases[i]);
	}
	free(osc_ctx->sine_waves);
	free(osc_ctx->cosine_waves);
	free(osc_ctx->phases);
}
