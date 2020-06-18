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

// function header
// from http://www.rds.org.uk/2010/pdf/RDS%202%20-%20what%20it%20is_170127_13.pdf
uint8_t fh = 34 << 2 | 0;

// see https://www.youtube.com/watch?v=ticcJpCPoa8
void get_logo_group(uint16_t *blocks) {
	static int logo_pos;

	blocks[0] = fh << 8 | station_logo[logo_pos];
	blocks[1] = station_logo[logo_pos+1] << 8 | station_logo[logo_pos+2];
	blocks[2] = station_logo[logo_pos+3] << 8 | station_logo[logo_pos+4];
	blocks[3] = station_logo[logo_pos+5] << 8 | station_logo[logo_pos+6];
	if ((logo_pos += 7) >= station_logo_len) logo_pos = 0;
}

void get_rds2_stream1_group(uint16_t *blocks) {
	get_logo_group(blocks);
}

void get_rds2_stream2_group(uint16_t *blocks) {
	get_logo_group(blocks);
}

void get_rds2_stream3_group(uint16_t *blocks) {
	get_logo_group(blocks);
}

void get_rds2_bits(int stream, int *out_buffer) {
    uint16_t out_blocks[GROUP_LENGTH];

    switch (stream) {
    case 1:
	get_rds2_stream1_group(out_blocks);
	break;
    case 2:
	get_rds2_stream3_group(out_blocks);
	break;
    case 3:
	get_rds2_stream3_group(out_blocks);
	break;
    }

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

float get_rds2_stream1_sample() {
    static int bit_buffer[BITS_PER_GROUP];
    static int bit_pos = BITS_PER_GROUP;
    static float sample_buffer[SAMPLE_BUFFER_SIZE];

    static int prev_output;
    static int cur_output;
    static int cur_bit;
    static int sample_count = SAMPLES_PER_BIT;
    static int inverting;

    static int in_sample_index;
    static int out_sample_index = SAMPLE_BUFFER_SIZE-1;

    if(sample_count == SAMPLES_PER_BIT) {
        if(bit_pos == BITS_PER_GROUP) {
            get_rds2_bits(1, bit_buffer);
            bit_pos = 0;
        }

        // do differential encoding
        cur_bit = bit_buffer[bit_pos];
        prev_output = cur_output;
        cur_output = prev_output ^ cur_bit;

        inverting = (cur_output == 1);

        float *src = waveform_biphase;
        int idx = in_sample_index;

        for(int j=0; j<FILTER_SIZE; j++) {
            float val = (*src++);
            if(inverting) val = -val;
            sample_buffer[idx++] += val;
            if(idx == SAMPLE_BUFFER_SIZE) idx = 0;
        }

        in_sample_index += SAMPLES_PER_BIT;
        if(in_sample_index == SAMPLE_BUFFER_SIZE) in_sample_index -= SAMPLE_BUFFER_SIZE;

        bit_pos++;
        sample_count = 0;
    }

    float sample = sample_buffer[out_sample_index];

    sample_buffer[out_sample_index++] = 0;
    if(out_sample_index == SAMPLE_BUFFER_SIZE) out_sample_index = 0;

    sample_count++;
    return sample;
}

float get_rds2_stream2_sample() {
    static int bit_buffer[BITS_PER_GROUP];
    static int bit_pos = BITS_PER_GROUP;
    static float sample_buffer[SAMPLE_BUFFER_SIZE];

    static int prev_output;
    static int cur_output;
    static int cur_bit;
    static int sample_count = SAMPLES_PER_BIT;
    static int inverting;

    static int in_sample_index;
    static int out_sample_index = SAMPLE_BUFFER_SIZE-1;

    if(sample_count == SAMPLES_PER_BIT) {
        if(bit_pos == BITS_PER_GROUP) {
            get_rds2_bits(2, bit_buffer);
            bit_pos = 0;
        }

        // do differential encoding
        cur_bit = bit_buffer[bit_pos];
        prev_output = cur_output;
        cur_output = prev_output ^ cur_bit;

        inverting = (cur_output == 1);

        float *src = waveform_biphase;
        int idx = in_sample_index;

        for(int j=0; j<FILTER_SIZE; j++) {
            float val = (*src++);
            if(inverting) val = -val;
            sample_buffer[idx++] += val;
            if(idx == SAMPLE_BUFFER_SIZE) idx = 0;
        }

        in_sample_index += SAMPLES_PER_BIT;
        if(in_sample_index == SAMPLE_BUFFER_SIZE) in_sample_index -= SAMPLE_BUFFER_SIZE;

        bit_pos++;
        sample_count = 0;
    }

    float sample = sample_buffer[out_sample_index];

    sample_buffer[out_sample_index++] = 0;
    if(out_sample_index == SAMPLE_BUFFER_SIZE) out_sample_index = 0;

    sample_count++;
    return sample;
}

float get_rds2_stream3_sample() {
    static int bit_buffer[BITS_PER_GROUP];
    static int bit_pos = BITS_PER_GROUP;
    static float sample_buffer[SAMPLE_BUFFER_SIZE];

    static int prev_output;
    static int cur_output;
    static int cur_bit;
    static int sample_count = SAMPLES_PER_BIT;
    static int inverting;

    static int in_sample_index;
    static int out_sample_index = SAMPLE_BUFFER_SIZE-1;

    if(sample_count == SAMPLES_PER_BIT) {
        if(bit_pos == BITS_PER_GROUP) {
            get_rds2_bits(3, bit_buffer);
            bit_pos = 0;
        }

        // do differential encoding
        cur_bit = bit_buffer[bit_pos];
        prev_output = cur_output;
        cur_output = prev_output ^ cur_bit;

        inverting = (cur_output == 1);

        float *src = waveform_biphase;
        int idx = in_sample_index;

        for(int j=0; j<FILTER_SIZE; j++) {
            float val = (*src++);
            if(inverting) val = -val;
            sample_buffer[idx++] += val;
            if(idx == SAMPLE_BUFFER_SIZE) idx = 0;
        }

        in_sample_index += SAMPLES_PER_BIT;
        if(in_sample_index == SAMPLE_BUFFER_SIZE) in_sample_index -= SAMPLE_BUFFER_SIZE;

        bit_pos++;
        sample_count = 0;
    }

    float sample = sample_buffer[out_sample_index];

    sample_buffer[out_sample_index++] = 0;
    if(out_sample_index == SAMPLE_BUFFER_SIZE) out_sample_index = 0;

    sample_count++;
    return sample;
}
