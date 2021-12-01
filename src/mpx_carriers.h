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

// context for MPX oscillator
typedef struct osc_ctx {
	uint8_t num_carriers;

	/*
	 * Arrays of carrier wave constants
	 *
	 */
	float **sine_waves;
	float **cosine_waves;

	/*
	 * Wave phase
	 *
	 */
	uint16_t **phases;
} osc_ctx;

enum phase_index {
	CURRENT,
	MAX
};

extern void init_mpx_carriers(struct osc_ctx *ctx, uint32_t sample_rate, const float *c_freqs);
extern float get_carrier(struct osc_ctx *ctx, uint8_t num);
extern float get_cos_carrier(struct osc_ctx *ctx, uint8_t num);
extern void update_carrier_phase(struct osc_ctx *ctx);
extern void exit_mpx_carriers(struct osc_ctx *ctx);
