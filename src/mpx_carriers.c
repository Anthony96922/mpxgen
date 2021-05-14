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
static int create_carrier(float freq, float *carrier, float *cos_carrier) {
	float sample, cos_sample;
	int sine_zero_crossings = 0;
	int i;

	// First value of a sine wave is always 0
	*carrier++ = 0;
	*cos_carrier++ = 1;

	for (i = 1; i < MPX_SAMPLE_RATE; i++) {
		sample = sin(2 * M_PI * freq * i / MPX_SAMPLE_RATE);
		cos_sample = cos(2 * M_PI * freq * i / MPX_SAMPLE_RATE);
		if (sample > -0.1e-6 && sample < 0.1e-6) {
			if (++sine_zero_crossings == 2) break;
			*carrier++ = 0;
		} else {
			*carrier++ = sample;
		}
		*cos_carrier++ = cos_sample;
	}

	return i;
}

#define NUM_CARRIERS 6
static float carrier_frequencies[NUM_CARRIERS] = {
	19000, 38000, 57000,
	66500, 71250, 76000 // RDS 2
};
static float *carrier[NUM_CARRIERS];
static float *cos_carrier[NUM_CARRIERS];
static int phase[NUM_CARRIERS][2]; // [carrier][current phase/max phase]

void create_mpx_carriers() {
	for (int i = 0; i < NUM_CARRIERS; i++) {
		carrier[i] = malloc(MPX_SAMPLE_RATE * sizeof(float));
		cos_carrier[i] = malloc(MPX_SAMPLE_RATE * sizeof(float));
		phase[i][1] = create_carrier(carrier_frequencies[i], carrier[i], cos_carrier[i]);
	}
}

void clear_mpx_carriers() {
	for (int i = 0; i < NUM_CARRIERS; i++) {
		free(carrier[i]);
		free(cos_carrier[i]);
	}
}

float get_carrier(int carrier_num) {
	return carrier[carrier_num][phase[carrier_num][0]];
}

float get_cos_carrier(int carrier_num) {
	return cos_carrier[carrier_num][phase[carrier_num][0]];
}

void update_carrier_phase() {
	for (int i = 0; i < NUM_CARRIERS; i++) {
		if (++phase[i][0] == phase[i][1]) phase[i][0] = 0;
	}
}
