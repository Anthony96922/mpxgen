/*
 * mpxgen - FM multiplex encoder with Stereo and RDS
 * Copyright (C) 2019-2021 Anthony96922
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

/*
 * Object for a Hilbert transform filter
 *
 */
typedef struct hilbert_fir_t {
	float *coeffs;
	float *in_buffer;
	uint16_t num_coeffs;
	float gain;
	uint16_t flt_buffer_idx;
} hilbert_fir_t;

extern void init_hilbert_transformer(struct hilbert_fir_t *flt, uint16_t size);
extern float get_hilbert(struct hilbert_fir_t *flt, float in);
extern void exit_hilbert_transformer(struct hilbert_fir_t *flt);
