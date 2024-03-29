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

// Audio in
#define NUM_AUDIO_FRAMES_IN	512
#define NUM_AUDIO_FRAMES_OUT	(NUM_AUDIO_FRAMES_IN * 8)

// MPX
#define NUM_MPX_FRAMES_IN	NUM_AUDIO_FRAMES_OUT
#define NUM_MPX_FRAMES_OUT	(NUM_MPX_FRAMES_IN * 2)

// The sample rate at which the MPX generation runs at
#define MPX_SAMPLE_RATE		190000

#define OUTPUT_SAMPLE_RATE	192000

/*
 * 2-channel FIR filter struct
 *
 */
typedef struct filter_t {
	uint32_t sample_rate;
	uint16_t index;
	uint16_t size;
	uint16_t half_size;
	float *in[2];

	// coefficients of the low-pass FIR filter
	float *filter;

	float out[2];
} filter_t;

/*
 * Filter delay line
 *
 */
typedef struct delay_line_t {
	float *buffer;
	uint32_t delay;
	uint32_t idx;
} delay_line_t;

extern void fm_mpx_init();
extern void fm_mpx_get_samples(float *in, float *out);
extern void fm_rds_get_samples(float *out);
extern void fm_mpx_exit();
extern void set_output_volume(uint8_t vol);
extern void set_carrier_volume(uint8_t carrier, uint8_t new_volume);
