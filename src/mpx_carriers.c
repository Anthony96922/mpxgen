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
 * code for MPX oscillator
 *
 */

// Create wave constants for a given frequency
static void create_carrier(uint32_t rate, float freq, float *sin_wave, float *cos_wave, uint16_t *max_phase) {
	float sin_sample, cos_sample;
	// used to determine if we have completed a cycle
	uint8_t zero_crossings = 0;
	uint16_t i;
	double w = 2.0 * M_PI * freq;
	double phase;

	// First value of a sine wave is always 0
	*sin_wave++ = 0.0;
	*cos_wave++ = 1.0;

	for (i = 1; i < rate; i++) {
		phase = i / (double)rate;
		sin_sample = sin(w * phase);
		cos_sample = cos(w * phase);
		if (sin_sample > -0.1e-4 && sin_sample < 0.1e-4) {
			if (++zero_crossings == 2) break;
			*sin_wave++ = 0.0;
		} else {
			*sin_wave++ = sin_sample;
		}
		*cos_wave++ = cos_sample;
	}

	*max_phase = i;
}

void init_mpx_carriers(struct osc_ctx *mpx_osc_ctx, uint32_t sample_rate, const float *c_freqs) {
	uint8_t num_carriers = 0;
	// look for the terminator (0)
	for (;;) {
		if (c_freqs[num_carriers] == 0.0) break;
		num_carriers++;
	}

	mpx_osc_ctx->num_carriers = num_carriers;

	mpx_osc_ctx->sine_waves = malloc(num_carriers * sizeof(float));
	mpx_osc_ctx->cosine_waves = malloc(num_carriers * sizeof(float));
	mpx_osc_ctx->phases = malloc(num_carriers * 2 * sizeof(uint16_t));

	for (uint8_t i = 0; i < num_carriers; i++) {
		mpx_osc_ctx->sine_waves[i] = malloc(sample_rate * sizeof(float));
		mpx_osc_ctx->cosine_waves[i] = malloc(sample_rate * sizeof(float));
		mpx_osc_ctx->phases[i] = malloc(2 * sizeof(uint16_t));
		mpx_osc_ctx->phases[i][CURRENT] = 0;

		// create waveform constants and load them into our oscillator
		create_carrier(sample_rate, c_freqs[i],
			mpx_osc_ctx->sine_waves[i],
			mpx_osc_ctx->cosine_waves[i],
			&mpx_osc_ctx->phases[i][MAX]
		);
	}
}

void exit_mpx_carriers(struct osc_ctx *mpx_osc_ctx) {
	for (uint8_t i = 0; i < mpx_osc_ctx->num_carriers; i++) {
		free(mpx_osc_ctx->sine_waves[i]);
		free(mpx_osc_ctx->cosine_waves[i]);
		free(mpx_osc_ctx->phases[i]);
	}
	free(mpx_osc_ctx->sine_waves);
	free(mpx_osc_ctx->cosine_waves);
	free(mpx_osc_ctx->phases);
}

float get_carrier(struct osc_ctx *mpx_osc_ctx, uint8_t carrier_num) {
	uint16_t cur_phase = mpx_osc_ctx->phases[carrier_num][CURRENT];
	return mpx_osc_ctx->sine_waves[carrier_num][cur_phase];
}

float get_cos_carrier(struct osc_ctx *mpx_osc_ctx, uint8_t carrier_num) {
	uint16_t cur_phase = mpx_osc_ctx->phases[carrier_num][CURRENT];
	return mpx_osc_ctx->cosine_waves[carrier_num][cur_phase];
}

void update_carrier_phase(struct osc_ctx *mpx_osc_ctx) {
	uint16_t cur_phase, max_phase;
	for (uint8_t i = 0; i < mpx_osc_ctx->num_carriers; i++) {
		cur_phase = mpx_osc_ctx->phases[i][CURRENT]++;
		max_phase = mpx_osc_ctx->phases[i][MAX];
		if (cur_phase == max_phase)
			mpx_osc_ctx->phases[i][CURRENT] = 0;
	}
}
