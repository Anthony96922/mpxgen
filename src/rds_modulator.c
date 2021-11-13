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
#include "rds.h"
#ifdef RDS2
#include "rds2.h"
#endif
#include "fm_mpx.h"
#include "waveforms.h"

static float *sym_waveforms[2];

/*
 * Also create the inverted version of the symbol waveform
 */
void init_symbol_waveforms() {
	for (int i = 0; i < 2; i++) {
		sym_waveforms[i] = malloc(FILTER_SIZE * sizeof(float));
		for (int j = 0; j < FILTER_SIZE; j++) {
			if (i) {
				sym_waveforms[i][j] = +waveform_biphase[j];
			} else {
				sym_waveforms[i][j] = -waveform_biphase[j];
			}
		}
	}
}

void exit_symbol_waveforms() {
	for (int i = 0; i < 2; i++) {
		free(sym_waveforms[i]);
	}
}

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

static struct rds_context rds_contexts[4];

/* Get an RDS sample. This generates the envelope of the waveform using
 * pre-generated elementary waveform samples.
 */
float get_rds_sample(uint8_t stream_num) {
	struct rds_context *rds = &rds_contexts[stream_num];

	if (rds->sample_count == SAMPLES_PER_BIT) {
		if (rds->bit_pos == BITS_PER_GROUP) {
#ifdef RDS2
			if (stream_num > 0) {
				get_rds2_bits(stream_num, rds->bit_buffer);
			} else {
				get_rds_bits(rds->bit_buffer);
			}
#else
			get_rds_bits(rds->bit_buffer);
#endif
			rds->bit_pos = 0;
		}

		// do differential encoding
		rds->cur_bit = rds->bit_buffer[rds->bit_pos++];
		rds->prev_output = rds->cur_output;
		rds->cur_output = rds->prev_output ^ rds->cur_bit;

		uint16_t idx = rds->in_sample_index;

		for (int j = 0; j < FILTER_SIZE; j++) {
			if (rds->cur_output) {
				rds->sample_buffer[idx] += sym_waveforms[1][j];
			} else {
				rds->sample_buffer[idx] += sym_waveforms[0][j];
			}
			idx++;
			if (idx == SAMPLE_BUFFER_SIZE) idx = 0;
		}

		rds->in_sample_index += SAMPLES_PER_BIT;
		if (rds->in_sample_index == SAMPLE_BUFFER_SIZE) {
			rds->in_sample_index = 0;
		}

		rds->sample_count = 0;
	}
	rds->sample_count++;

	rds->sample = rds->sample_buffer[rds->out_sample_index];
	rds->sample_buffer[rds->out_sample_index++] = 0;
	if (rds->out_sample_index >= SAMPLE_BUFFER_SIZE) {
		rds->out_sample_index = 0;
	}

	return rds->sample;
}
