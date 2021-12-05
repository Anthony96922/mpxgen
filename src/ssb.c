/*
 * mpxgen - FM multiplex encoder with Stereo and RDS
 * Copyright (C) 2021 Anthony96922
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
#include "ssb.h"

/*
 * Hilbert transform FIR filter
 *
 * https://www-users.cs.york.ac.uk/~fisher/mkfilter/
 * http://dp.nonoo.hu/projects/ham-dsp-tutorial/09-ssb-hartley/
 *
 * Filter creation based on the code from
 * https://github.com/MikeCurrington/mkfilter/
 */

void init_hilbert_transformer(struct hilbert_fir_t *flt, uint16_t size) {
	uint16_t half_size = size / 2;
	double filter, window;
	uint8_t odd = 0;

	memset(flt, 0, sizeof(struct hilbert_fir_t));
	flt->num_coeffs = size + 1;
	flt->coeffs = malloc(flt->num_coeffs * sizeof(float));
	flt->in_buffer = malloc(flt->num_coeffs * sizeof(float));

	// start from the center
	for (uint16_t i = 1; i < half_size + 1; i++) {
		if (i & 1) { // calculate for odd indexes only
			filter = 1.0 / (double)i;
			// Hamming window
			window = 0.54 - 0.46 * cos(M_2PI * (double)(half_size + i) / (double)size);
			flt->coeffs[half_size+i] = (float)(-filter * window);
			flt->coeffs[half_size-i] = (float)(+filter * window);
		} else {
			flt->coeffs[half_size+i] = 0.0f;
			flt->coeffs[half_size-i] = 0.0f;
		}
	}

	// set center of filter to 0
	flt->coeffs[half_size] = 0.0;

	// calculate input gain
	for (uint16_t i = half_size & 1 ? 0 : 1; i < flt->num_coeffs && flt->coeffs[i] > 0.0; i += 2) {
		flt->gain += (odd) ? +flt->coeffs[i] : -flt->coeffs[i];
		odd ^= 1;
	}

	if (odd) flt->gain = -flt->gain;
	flt->gain *= 2.0f;

#if 0
	printf("coeffs: ");
	for (int i = 0; i < flt->num_coeffs; i++) {
		printf("%.7f, ", flt->coeffs[i]);
	}
	printf("\ngain: %.7f\n", flt->gain);
#endif
}

float get_hilbert(struct hilbert_fir_t *flt, float in) {
	float filter_out;
	uint16_t filter_idx;

	flt->in_buffer[flt->flt_buffer_idx++] = in / flt->gain;
	if (flt->flt_buffer_idx == flt->num_coeffs) flt->flt_buffer_idx = 0;

	filter_idx = flt->flt_buffer_idx;

	filter_out = 0.0f;
	for (uint16_t i = 0; i < flt->num_coeffs; i++) {
		filter_out += flt->in_buffer[filter_idx++] * flt->coeffs[i];
		if (filter_idx == flt->num_coeffs) filter_idx = 0;
	}

	return filter_out;
}

void exit_hilbert_transformer(struct hilbert_fir_t *flt) {
	free(flt->coeffs);
	free(flt->in_buffer);
}
