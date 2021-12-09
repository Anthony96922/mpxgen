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
typedef struct osc_t {
	/*
	 * number of freqs the struct will store waveforms for
	 *
	 */
	uint8_t num_freqs;

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
} osc_t;

// oscillator phase index
enum osc_phase_index {
	CURRENT,
	MAX
};

extern void init_osc(struct osc_t *osc_ctx, uint32_t sample_rate, const float *c_freqs);
extern float get_wave(struct osc_t *osc_ctx, uint8_t num, uint8_t cosine);
extern void update_osc_phase(struct osc_t *osc_ctx);
extern void exit_osc(struct osc_t *osc_ctx);
