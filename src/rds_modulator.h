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

#include "rds.h"

// RDS signal context
typedef struct rds_context {
	uint8_t bit_buffer[BITS_PER_GROUP];
	uint8_t bit_pos;
	float sample_buffer[SAMPLE_BUFFER_SIZE];
	uint8_t prev_output;
	uint8_t cur_output;
	uint8_t cur_bit;
	uint8_t sample_count;
	uint16_t in_sample_index;
	uint16_t out_sample_index;
	float sample_pos;
	float sample;
} rds_context;

extern void init_symbol_waveforms();
extern void exit_symbol_waveforms();
