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

/* offset words */
extern uint16_t offset_words[];

/* our crc function is in rds.c */
extern uint16_t crc(uint16_t blocks);

/* RDS2-specific stuff
 */

// station logo
extern unsigned char station_logo[];
extern unsigned int station_logo_len;

/* Function Header
   (from http://www.rds.org.uk/2010/pdf/RDS%202%20-%20what%20it%20is_170127_13.pdf)
   I have not idea what this is for
 */
uint8_t fh = 34 << 2 | 0;

// See https://www.youtube.com/watch?v=ticcJpCPoa8
void get_logo_group(uint16_t *blocks) {
	static int logo_pos;

	blocks[0] = fh << 8 | station_logo[logo_pos];
	blocks[1] = station_logo[logo_pos+1] << 8 | station_logo[logo_pos+2];
	blocks[2] = station_logo[logo_pos+3] << 8 | station_logo[logo_pos+4];
	blocks[3] = station_logo[logo_pos+5] << 8 | station_logo[logo_pos+6];
	if ((logo_pos += 7) >= station_logo_len) logo_pos = 0;
}

void get_rds2_group(int stream_num, uint16_t *blocks) {
    switch (stream_num) {
    case 0:
    case 1:
    case 2:
        get_logo_group(blocks);
        break;
    }
}

void get_rds2_bits(int stream, uint8_t *out_buffer) {
    uint16_t out_blocks[GROUP_LENGTH];

    get_rds2_group(stream, out_blocks);

    //fprintf(stderr, "Stream %d: %04x %04x %04x %04x\n", stream, out_blocks[0], out_blocks[1], out_blocks[2], out_blocks[3]);

    // Calculate the checkword for each block and emit the bits
    uint16_t block, check;
    for(int i=0; i<GROUP_LENGTH; i++) {
        block = out_blocks[i];
        check = crc(block) ^ offset_words[i];
        for(int j=0; j<BLOCK_SIZE; j++) {
            *out_buffer++ = ((block & (1<<(BLOCK_SIZE-1))) != 0);
            block <<= 1;
        }
        for(int j=0; j<POLY_DEG; j++) {
            *out_buffer++ = ((check & (1<<(POLY_DEG-1))) != 0);
            check <<= 1;
        }
    }
}

float get_rds2_sample(int stream_num) {
    static uint8_t bit_buffer[3][BITS_PER_GROUP];
    static uint8_t bit_pos[3] = {BITS_PER_GROUP, BITS_PER_GROUP, BITS_PER_GROUP};
    static float sample_buffer[3][SAMPLE_BUFFER_SIZE];

    static int prev_output[3];
    static int cur_output[3];
    static int cur_bit[3];
    static int sample_count[3] = {SAMPLES_PER_BIT, SAMPLES_PER_BIT, SAMPLES_PER_BIT};
    static int inverting[3];

    static int in_sample_index[3];
    static int out_sample_index[3] = {SAMPLE_BUFFER_SIZE, SAMPLE_BUFFER_SIZE, SAMPLE_BUFFER_SIZE};

    stream_num--;

    if(sample_count[stream_num] == SAMPLES_PER_BIT) {
        if(bit_pos[stream_num] == BITS_PER_GROUP) {
            get_rds2_bits(stream_num, bit_buffer[stream_num]);
            bit_pos[stream_num] = 0;
        }


        // do differential encoding
        cur_bit[stream_num] = bit_buffer[stream_num][bit_pos[stream_num]];
        prev_output[stream_num] = cur_output[stream_num];
        cur_output[stream_num] = prev_output[stream_num] ^ cur_bit[stream_num];

        inverting[stream_num] = (cur_output[stream_num] == 1);

        int idx = in_sample_index[stream_num];

        for(int j=0; j<FILTER_SIZE; j++) {
            float val = waveform_biphase[j];
            if(inverting[stream_num]) val = -val;
            sample_buffer[stream_num][idx++] += val;
            if(idx == SAMPLE_BUFFER_SIZE) idx = 0;
        }

        in_sample_index[stream_num] += SAMPLES_PER_BIT;
        if(in_sample_index[stream_num] == SAMPLE_BUFFER_SIZE) in_sample_index[stream_num] = 0;

        bit_pos[stream_num]++;
        sample_count[stream_num] = 0;
    }

    float sample = sample_buffer[stream_num][out_sample_index[stream_num]];

    sample_buffer[stream_num][out_sample_index[stream_num]++] = 0;
    if(out_sample_index[stream_num] >= SAMPLE_BUFFER_SIZE) out_sample_index[stream_num] = 0;

    sample_count[stream_num]++;
    return sample;
}
