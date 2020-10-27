/*
 * mpxgen - FM multiplex encoder with Stereo and RDS
 * Copyright (C) 2019-2020 Anthony96922
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

#include <stdio.h>
#include "rds.h"
#include "waveforms.h"

/*
 * RDS2-specific stuff
 */

// station logo
extern unsigned char station_logo[];
extern unsigned int station_logo_len;

// RDS signal context
typedef struct {
	uint8_t bit_buffer[BITS_PER_GROUP];
	uint8_t bit_pos;
	float sample_buffer[SAMPLE_BUFFER_SIZE];
	int prev_output;
	int cur_output;
	int cur_bit;
	int sample_count;
	int inverting;
	int in_sample_index;
	int out_sample_index;
} rds_signal_context;

rds_signal_context rds2_contexts[3];

/*
 * Function Header
 *
 * (from http://www.rds.org.uk/2010/pdf/RDS%202%20-%20what%20it%20is_170127_13.pdf)
 * I have not idea what this is for
 */
uint8_t fh = 34 << 2 | 0;

/*
 * Station logo group (not fully implemented)
 * See https://www.youtube.com/watch?v=ticcJpCPoa8
 */
void get_logo_group(uint16_t *blocks) {
	static int logo_pos;

	blocks[0] = fh << 8 | station_logo[logo_pos];
	blocks[1] = station_logo[logo_pos+1] << 8 | station_logo[logo_pos+2];
	blocks[2] = station_logo[logo_pos+3] << 8 | station_logo[logo_pos+4];
	blocks[3] = station_logo[logo_pos+5] << 8 | station_logo[logo_pos+6];
	if ((logo_pos += 7) >= station_logo_len) logo_pos = 0;
}

/*
 * RDS 2 group sequence
 */
void get_rds2_group(int stream_num, uint16_t *blocks) {
	switch (stream_num) {
	case 0:
	case 1:
	case 2:
		get_logo_group(blocks);
		break;
	}

	//fprintf(stderr, "Stream %d: %04x %04x %04x %04x\n",
	//	stream_num, blocks[0], blocks[1], blocks[2], blocks[3]);
}

void get_rds2_bits(int stream, uint8_t *out_buffer) {
	uint16_t out_blocks[GROUP_LENGTH] = {0};
	get_rds2_group(stream, out_blocks);
	add_checkwords(out_blocks, out_buffer);
}

/*
 * Creates the RDS 2 signal. Like get_rds_sample, but generates the signal
 * for a chosen stream number.
 */
float get_rds2_sample(int stream_num) {
	// select context from stream number
	rds_signal_context *rds2 = &rds2_contexts[--stream_num];

	if(rds2->sample_count == SAMPLES_PER_BIT) {
		if(rds2->bit_pos == BITS_PER_GROUP) {
			get_rds2_bits(stream_num, rds2->bit_buffer);
			rds2->bit_pos = 0;
		}

		// do differential encoding
		rds2->cur_bit = rds2->bit_buffer[rds2->bit_pos++];
		rds2->prev_output = rds2->cur_output;
		rds2->cur_output = rds2->prev_output ^ rds2->cur_bit;

		rds2->inverting = (rds2->cur_output == 1);

		int idx = rds2->in_sample_index;

		for(int j=0; j<WAVEFORM_SIZE; j++) {
			rds2->sample_buffer[idx++] +=
				(!rds2->inverting) ? waveform_biphase[j] : -waveform_biphase[j];
			if(idx == SAMPLE_BUFFER_SIZE) idx = 0;
		}

		rds2->in_sample_index += SAMPLES_PER_BIT;
		if(rds2->in_sample_index == SAMPLE_BUFFER_SIZE) rds2->in_sample_index = 0;

		rds2->sample_count = 0;
	}

	float sample = rds2->sample_buffer[rds2->out_sample_index];

	rds2->sample_buffer[rds2->out_sample_index++] = 0;
	if(rds2->out_sample_index >= SAMPLE_BUFFER_SIZE) rds2->out_sample_index = 0;
	rds2->sample_count++;

	return sample;
}
