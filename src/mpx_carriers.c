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

// Create wave constants
int create_carrier(int sample_rate, float freq, float *carrier) {
	float sample;
	int sine_zero_crossings = 0;
	int i;

	// First value of a sine wave is always 0
	*carrier++ = 0;

	for (i = 1; i < sample_rate; i++) {
		sample = sin(2 * M_PI * freq * i / sample_rate);
		if (sample > -0.1e-7 && sample < 0.1e-7) {
			if (++sine_zero_crossings == 2) break;
			*carrier++ = 0;
		} else {
			*carrier++ = sample;
		}
	}

	return i;
}

float *carrier_19k;
float *carrier_38k;
float *carrier_57k;
int max_19k;
int max_38k;
int max_57k;

/* RDS 2 carriers
 * 66.5/71.25/76
 */
#ifdef RDS2
float *carrier_67k;
float *carrier_71k;
float *carrier_76k;
int max_67k;
int max_71k;
int max_76k;
#endif

void create_mpx_carriers(int sample_rate) {
	carrier_19k = malloc(sample_rate * sizeof(float));
	carrier_38k = malloc(sample_rate * sizeof(float));
	carrier_57k = malloc(sample_rate * sizeof(float));
	max_19k = create_carrier(sample_rate, 19000, carrier_19k);
	max_38k = create_carrier(sample_rate, 38000, carrier_38k);
	max_57k = create_carrier(sample_rate, 57000, carrier_57k);
#ifdef RDS2
	carrier_67k = malloc(sample_rate * sizeof(float));
	carrier_71k = malloc(sample_rate * sizeof(float));
	carrier_76k = malloc(sample_rate * sizeof(float));
	max_67k = create_carrier(sample_rate, 66500, carrier_67k);
	max_71k = create_carrier(sample_rate, 71250, carrier_71k);
	max_76k = create_carrier(sample_rate, 76000, carrier_76k);
#endif
}

void clear_mpx_carriers() {
	free(carrier_19k);
	free(carrier_38k);
	free(carrier_57k);
#ifdef RDS2
	free(carrier_67k);
	free(carrier_71k);
	free(carrier_76k);
#endif
}

int phase_19k;
int phase_38k;
int phase_57k;
#ifdef RDS2
int phase_67k;
int phase_71k;
int phase_76k;
#endif

float level_19k = 1;
float level_38k = 1;
float level_57k = 1;

float get_19k_carrier() {
	return carrier_19k[phase_19k] * level_19k;
}

float get_38k_carrier() {
	return carrier_38k[phase_38k] * level_38k;
}

float get_57k_carrier() {
	return carrier_57k[phase_57k] * level_57k;
}

#ifdef RDS2
float get_67k_carrier() {
	return carrier_67k[phase_67k];
}

float get_71k_carrier() {
	return carrier_71k[phase_71k];
}

float get_76k_carrier() {
	return carrier_76k[phase_76k];
}
#endif

void update_carrier_phase() {
	if (++phase_19k == max_19k) phase_19k = 0;
	if (++phase_38k == max_38k) phase_38k = 0;
	if (++phase_57k == max_57k) phase_57k = 0;
#ifdef RDS2
	if (++phase_67k == max_67k) phase_67k = 0;
	if (++phase_71k == max_71k) phase_71k = 0;
	if (++phase_76k == max_76k) phase_76k = 0;
#endif
}

void set_19k_level(int new_level) {
	if (new_level == -1) return;
	level_19k = (new_level / 100.0);
}

void set_38k_level(int new_level) {
	if (new_level == -1) return;
	level_38k = (new_level / 100.0);
}

void set_57k_level(int new_level) {
	if (new_level == -1) return;
	level_57k = (new_level / 100.0);
}
