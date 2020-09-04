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

// Create wave constants for a given frequency and sample rate
int create_carrier(int sample_rate, float freq, float *carrier) {
	float sample;
	int sine_zero_crossings = 0;
	int i;

	// First value of a sine wave is always 0
	*carrier++ = 0;

	for (i = 1; i < sample_rate; i++) {
		sample = sin(2 * M_PI * freq * i / sample_rate);
		if (sample > -0.1e-6 && sample < 0.1e-6) {
			if (++sine_zero_crossings == 2) break;
			*carrier++ = 0;
		} else {
			*carrier++ = sample;
		}
	}

	return i;
}

/* RDS 2 carriers
 * 66.5/71.25/76
 */
#ifdef RDS2
int num_carriers = 6;
float carrier_frequencies[6] = {19000, 38000, 57000, 66500, 71250, 76000};
float *carrier[6];
int phase[6][2];
float level[6];
#else
int num_carriers = 4;
float carrier_frequencies[4] = {19000, 38000, 57000, 31250};
float *carrier[4];
int phase[4][2]; // [carrier][current phase/max phase]
float level[4];
#endif

void create_mpx_carriers(int sample_rate) {
	for (int i = 0; i < num_carriers; i++) {
		carrier[i] = malloc(sample_rate * sizeof(float));
		phase[i][1] = create_carrier(sample_rate, carrier_frequencies[i], carrier[i]);
		level[i] = 1;
	}
}

void clear_mpx_carriers() {
	for (int i = 0; i < num_carriers; i++) {
		free(carrier[i]);
	}
}

float get_carrier(int carrier_num) {
	return carrier[carrier_num][phase[carrier_num][0]] * level[carrier_num];
}

void update_carrier_phase() {
	for (int i = 0; i < num_carriers; i++) {
		if (++phase[i][0] == phase[i][1]) phase[i][0] = 0;
	}
}

void set_level(int carrier, int new_level) {
	if (new_level == -1) return;
	level[carrier] = (new_level / 100.0);
}
