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
#ifdef RDS2
#include "rds2.h"
#endif
#include "fm_mpx.h"
#include "waveforms.h"
#include "rds_modulator.h"

static float *sym_waveforms[2];

/*
 * Also create the inverted version of the symbol waveform
 */
void init_symbol_waveforms() {
	for (uint8_t i = 0; i < 2; i++) {
		sym_waveforms[i] = malloc(FILTER_SIZE * sizeof(float));
		for (uint16_t j = 0; j < FILTER_SIZE; j++) {
			sym_waveforms[i][j] = (i) ?
				+waveform_biphase[j] : -waveform_biphase[j];
		}
	}
}

void exit_symbol_waveforms() {
	for (uint8_t i = 0; i < 2; i++) {
		free(sym_waveforms[i]);
	}
}

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

		for (uint16_t j = 0; j < FILTER_SIZE; j++) {
			rds->sample_buffer[idx++] +=
				sym_waveforms[rds->cur_output][j];
			if (idx == SAMPLE_BUFFER_SIZE) idx = 0;
		}

		rds->in_sample_index += SAMPLES_PER_BIT;
		if (rds->in_sample_index == SAMPLE_BUFFER_SIZE)
			rds->in_sample_index = 0;

		rds->sample_count = 0;
	}
	rds->sample_count++;

	rds->sample = rds->sample_buffer[rds->out_sample_index];
	rds->sample_buffer[rds->out_sample_index++] = 0;
	if (rds->out_sample_index == SAMPLE_BUFFER_SIZE)
		rds->out_sample_index = 0;

	return rds->sample;
}
