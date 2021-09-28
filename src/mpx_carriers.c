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

/*
 * code for MPX oscillator
 *
 */

// Create wave constants for a given frequency
static void create_carrier(uint32_t rate, float freq, float *sin_wave, float *cos_wave, uint32_t *max_phase) {
	float sin_sample, cos_sample;
	// used to determine if we have completed a cycle
	uint8_t zero_crossings = 0;
	uint32_t i;
	float w = 2.0 * M_PI * freq;
	float phase;

	// First value of a sine wave is always 0
	*sin_wave++ = 0.0;
	*cos_wave++ = 1.0;

	for (i = 1; i < rate; i++) {
		phase = i / (float)rate;
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

static float carrier_frequencies[] = {
	19000.0, // pilot tone
	38000.0, // stereo difference
	57000.0, // RDS

#ifdef RDS2
	// RDS 2
	66500.0, // stream 1
	71250.0, // stream 2
	76000.0  // stream 2
#endif
};

#define NUM_CARRIERS sizeof(carrier_frequencies)/sizeof(float)

static float **sin_carrier;
static float **cos_carrier;

/*
 * Wave phase
 *
 * index 0: current phase
 * index 1: max phase
 */
static uint32_t **phase;

void init_mpx_carriers(uint32_t sample_rate) {
	sin_carrier = (float **)malloc(NUM_CARRIERS * sizeof(float));
	cos_carrier = (float **)malloc(NUM_CARRIERS * sizeof(float));
	phase = (uint32_t **)malloc(NUM_CARRIERS * 2 * sizeof(uint32_t));
	for (int i = 0; i < NUM_CARRIERS; i++) {
		sin_carrier[i] = malloc(sample_rate * sizeof(float));
		cos_carrier[i] = malloc(sample_rate * sizeof(float));
		phase[i] = malloc(2 * sizeof(uint32_t));
		phase[i][0] = 0;
		create_carrier(sample_rate, carrier_frequencies[i], sin_carrier[i], cos_carrier[i], &phase[i][1]);
	}
}

void exit_mpx_carriers() {
	for (int i = 0; i < NUM_CARRIERS; i++) {
		free(sin_carrier[i]);
		free(cos_carrier[i]);
		free(phase[i]);
	}
	free(sin_carrier);
	free(cos_carrier);
	free(phase);
}

float get_carrier(uint8_t carrier_num) {
	return sin_carrier[carrier_num][phase[carrier_num][0]];
}

float get_cos_carrier(uint8_t carrier_num) {
	return cos_carrier[carrier_num][phase[carrier_num][0]];
}

void update_carrier_phase() {
	for (int i = 0; i < NUM_CARRIERS; i++) {
		if (++phase[i][0] == phase[i][1]) phase[i][0] = 0;
	}
}
