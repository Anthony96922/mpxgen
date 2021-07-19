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

#include <stdlib.h>
#include <math.h>
#include "fm_mpx.h"

// Create wave constants for a given frequency
static void create_carrier(float freq, float *sin_wave, float *cos_wave, int *max_phase) {
	float sin_sample, cos_sample;

	// used to determine if we have completed a cycle
	int zero_crossings = 0;

	int i;

	// First value of a sine wave is always 0
	*sin_wave++ = 0;
	*cos_wave++ = 1;

	for (i = 1; i < MPX_SAMPLE_RATE; i++) {
		sin_sample = sin(2 * M_PI * freq * i / MPX_SAMPLE_RATE);
		cos_sample = cos(2 * M_PI * freq * i / MPX_SAMPLE_RATE);
		if (sin_sample > -0.1e-6 && sin_sample < 0.1e-6) {
			if (++zero_crossings == 2) break;
			*sin_wave++ = 0;
		} else {
			*sin_wave++ = sin_sample;
		}
		*cos_wave++ = cos_sample;
	}

	*max_phase = i;
}

static float carrier_frequencies[] = {
	19000, // pilot tone
	38000, // stereo difference
	57000, // RDS

#ifdef RDS2
	// RDS 2
	66500, // stream 1
	71250, // stream 2
	76000  // stream 2
#endif
};

#define NUM_CARRIERS sizeof(carrier_frequencies)/sizeof(float)

static float sin_carrier[NUM_CARRIERS][MPX_SAMPLE_RATE];
static float cos_carrier[NUM_CARRIERS][MPX_SAMPLE_RATE];

/*
 * Wave phase
 *
 * 0: current phase
 * 1: max phase
 */
static int phase[NUM_CARRIERS][2];

void create_mpx_carriers() {
	for (int i = 0; i < NUM_CARRIERS; i++) {
		create_carrier(carrier_frequencies[i], sin_carrier[i], cos_carrier[i], &phase[i][1]);
	}
}

void clear_mpx_carriers() {
	// no-op
}

float get_carrier(int carrier_num) {
	return sin_carrier[carrier_num][phase[carrier_num][0]];
}

float get_cos_carrier(int carrier_num) {
	return cos_carrier[carrier_num][phase[carrier_num][0]];
}

void update_carrier_phase() {
	for (int i = 0; i < NUM_CARRIERS; i++) {
		if (++phase[i][0] == phase[i][1]) phase[i][0] = 0;
	}
}
