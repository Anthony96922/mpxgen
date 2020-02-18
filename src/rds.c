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

#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#include "waveforms.h"
#include "fm_mpx.h"
#include "rds.h"

struct {
    uint16_t pi;
    int ta;
    int pty;
    int tp;
    int ms;
    int af[MAX_AF+1];
    // PS
    char ps[8];
    // RT
    char rt[64];
    // PTYN
    char ptyn[8];
} rds_params;
/* Here, the first member of the struct must be a scalar to avoid a
   warning on -Wmissing-braces with GCC < 4.8.3
   (bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119)
*/

// RDS data controls
struct {
    int ab;
    int ps_update;
    int rt_update;
    int ptyn_update;
    int enable_ptyn;
} rds_controls;

// RT+
struct {
    int running;
    int toggle;
    int type_1;
    int start_1;
    int len_1;
    int type_2;
    int start_2;
    int len_2;
} rtp_params;

uint16_t offset_words[] = {0x0FC, 0x198, 0x168, 0x1B4};
// We don't handle offset word C' here for the sake of simplicity

/* Classical CRC computation */
uint16_t crc(uint16_t block) {
    uint16_t crc = 0;

    for(int j=0; j<BLOCK_SIZE; j++) {
        int bit = (block & MSB_BIT) != 0;
        block <<= 1;

        int msb = (crc >> (POLY_DEG-1)) & 1;
        crc <<= 1;
        if((msb ^ bit) != 0) {
            crc ^= POLY;
        }
    }
    return crc;
}

/* Generates a CT (clock time) group if the minute has just changed
   Returns 1 if the CT group was generated, 0 otherwise
*/
int get_rds_ct_group(uint16_t *blocks) {
    static int latest_minutes = -1;

    // Check time
    time_t now;
    static struct tm *utc;

    now = time (NULL);
    utc = gmtime (&now);

    if(utc->tm_min != latest_minutes) {
        // Generate CT group
        latest_minutes = utc->tm_min;

        int l = utc->tm_mon <= 1 ? 1 : 0;
        int mjd = 14956 + utc->tm_mday +
                  (int)((utc->tm_year - l) * 365.25) +
                  (int)((utc->tm_mon + 2 + l*12) * 30.6001);

        blocks[1] |= 4 << 12 | (mjd>>15);
        blocks[2] = (mjd<<1) | (utc->tm_hour>>4);
        blocks[3] = (utc->tm_hour & 0xF)<<12 | utc->tm_min<<6;

        utc = localtime(&now);

        int offset = utc->tm_gmtoff / (30 * 60);
        blocks[3] |= abs(offset);
        if(offset < 0) blocks[3] |= 0x20;

        return 1;
    } else return 0;
}

/* PS group (0A)
 */
void get_rds_ps_group(uint16_t *blocks) {
	static char ps_text[8];
	static int ps_state, af_state;

	blocks[1] |= /* 0 << 12 | */ rds_params.ta << 4 | rds_params.ms << 3 | ps_state;
	if (ps_state == 3 && channels == 2) blocks[1] |= 4; // DI = 1 - Stereo
	if (rds_params.af[0]) { // AF
		if (af_state == 0) {
			blocks[2] = (rds_params.af[0] + 224) << 8 | rds_params.af[1];
		} else {
			blocks[2] = rds_params.af[af_state] << 8;
			blocks[2] |= rds_params.af[af_state+1] ?
					rds_params.af[af_state+1] : 0xCD;
		}
		af_state += 2;
		if (af_state > rds_params.af[0]) af_state = 0;
	} else {
		blocks[2] = 0xE0CD; // no AF
	}
	blocks[3] = ps_text[ps_state*2] << 8 | ps_text[ps_state*2+1];
	ps_state++;
	if (ps_state == 4) {
		ps_state = 0;
		if (rds_controls.ps_update) {
			strncpy(ps_text, rds_params.ps, 8);
			rds_controls.ps_update = 0;
		}
	}
}

/* RT group (2A)
 */
void get_rds_rt_group(uint16_t *blocks) {
	static char rt_text[64];
	static int rt_state;

start_over:
	if (rds_controls.rt_update) {
		strncpy(rt_text, rds_params.rt, 64);
		rds_controls.rt_update = 0;
		rt_state = 0;
	}

	if (rt_text[rt_state*4] == 0) {
		rt_state = 0;
		goto start_over;
	}

	blocks[1] |= 2 << 12 | rds_controls.ab << 4 | rt_state;
	blocks[2] = rt_text[rt_state*4+0] << 8 | rt_text[rt_state*4+1];
	blocks[3] = rt_text[rt_state*4+2] << 8 | rt_text[rt_state*4+3];

	rt_state++;
	if (rt_state == 16) rt_state = 0;
}

/* ODA group (3A)
 */
void get_rds_oda_group(uint16_t *blocks) {
	static int oda_state;

	blocks[1] |= 3 << 12;

	switch (oda_state++) {
	case 0: // RT+
		// Assign the RT+ AID to group 11A
		blocks[1] |= 11 << 1;
		blocks[3] = 0x4BD7; // RT+ AID
		break;
	}

	if (oda_state == 1) oda_state = 0;
}

/* PTYN group (10A)
 */
void get_rds_ptyn_group(uint16_t *blocks) {
	static char ptyn_text[8];
	static int ptyn_state;

	blocks[1] |= 10 << 12 | ptyn_state;
	blocks[2] = ptyn_text[ptyn_state*4+0] << 8 | ptyn_text[ptyn_state*4+1];
	blocks[3] = ptyn_text[ptyn_state*4+2] << 8 | ptyn_text[ptyn_state*4+3];
	ptyn_state++;
	if (ptyn_state == 2) {
		ptyn_state = 0;
		if (rds_controls.ptyn_update) {
			strncpy(ptyn_text, rds_params.ptyn, 8);
			rds_controls.ptyn_update = 0;
		}
	}
}

/* RT+ group (assigned to 11A)
 */
void get_rds_rtp_group(uint16_t *blocks) {
	// RT+ block format
	blocks[1] |= 11 << 12 | rtp_params.toggle << 4 | rtp_params.running << 3 |
		    (rtp_params.type_1 & 0x38) >> 3;
	blocks[2] = (rtp_params.type_1 & 0x7) << 13 | (rtp_params.start_1 & 0x3F) << 7 |
		    (rtp_params.len_1 & 0x3F) << 1 | (rtp_params.type_2 & 0x20) >> 5;
	blocks[3] = (rtp_params.type_2 & 0x1F) << 11 | (rtp_params.start_2 & 0x3F) << 5 |
		    (rtp_params.len_2 & 0x1F);
}

/* Lower priority groups are placed in a subsequence
 */
void get_rds_other_groups(uint16_t *blocks) {
	static int state;

next:
	switch (state) {
	case 0: // Type 3A groups
		get_rds_oda_group(blocks);
		break;
	case 1: // Type 10A groups
		// Do not generate a 10A group if PTYN is off
		if (!rds_controls.enable_ptyn) {
			state++;
			goto next;
		}
		get_rds_ptyn_group(blocks);
		break;
	case 2: // Type 11A groups
		get_rds_rtp_group(blocks);
		break;
	}

	state++;
	if (state == 3) state = 0;
}

/* Creates an RDS group. This generates sequences of the form 0A, 2A, 0A, 2A, 0A, 2A, etc.
*/
void get_rds_group(uint16_t *blocks) {
    static int state;

    // Basic block data
    blocks[0] = rds_params.pi;
    blocks[1] = rds_params.tp << 10 | rds_params.pty << 5;
    blocks[2] = 0;
    blocks[3] = 0;

    // Generate block content
    if(!get_rds_ct_group(blocks)) { // CT (clock time) has priority on other group types
	switch (state) {
	case 0:
	case 2:
	case 4:
	case 6:
	case 8:
	case 10: // Type 0A groups
	    get_rds_ps_group(blocks);
	    break;
	case 1:
	case 3:
	case 5:
	case 7:
	case 9:
	case 11: // Type 2A groups
	    get_rds_rt_group(blocks);
	    break;
	default: // Other groups
	    get_rds_other_groups(blocks);
	    break;
	}

	state++;
	if(state == 14) state = 0;
    }
}

void get_rds_bits(int *out_buffer) {
    static uint16_t out_blocks[GROUP_LENGTH];
    get_rds_group(out_blocks);

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

/* Get an RDS sample. This generates the envelope of the waveform using
   pre-generated elementary waveform samples.
 */
float get_rds_sample() {
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
            get_rds_bits(bit_buffer);
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

// RBDS PTY list
char *ptys[] = {
	"None", "News", "Information", "Sports",
	"Talk", "Rock", "Classic rock", "Adult hits",
	"Soft rock" , "Top 40", "Country", "Oldies",
	"Soft music", "Nostalgia", "Jazz", "Classical",
	"R&B", "Soft R&B", "Language", "Religious music",
	"Religious talk", "Personality", "Public", "College",
	"Spanish talk", "Spanish music", "Hip-Hop", "Unassigned",
	"Unassigned", "Weather", "Emergency test", "Emergency"
};

void rds_encoder_init(uint16_t pi, char *ps, char *rt, int pty, int tp, int *af_array, char *ptyn) {

    printf("RDS Options:\n");
    printf("PI: %04X, PS: \"%s\", PTY: %d (%s), TP: %d\n",
	pi, ps, pty, ptys[pty], tp);
    printf("RT: \"%s\"\n", rt);

    // AF
    if(af_array[0]) {
	set_rds_af(af_array);
	printf("AF: %d,", af_array[0]);
	for(int f = 1; f < af_array[0]+1; f++) {
	    printf(" %.1f", (float)(af_array[f]+875)/10);
	}
	printf("\n");
    }

    set_rds_pi(pi);
    set_rds_ps(ps);
    set_rds_ab(1);
    set_rds_rt(rt);
    set_rds_pty(pty);
    if (ptyn != NULL) {
	printf("PTYN: \"%s\"\n", ptyn);
	set_rds_ptyn(ptyn, 1);
    }
    set_rds_tp(tp);
    set_rds_ms(1);
}

void set_rds_pi(uint16_t pi_code) {
    rds_params.pi = pi_code;
}

void set_rds_rt(char *rt) {
    int rt_len = strlen(rt);

    rds_controls.rt_update = 1;
    rds_controls.ab ^= 1;
    strncpy(rds_params.rt, rt, 64);

    // Terminate RT with '\r' (carriage return) if RT is < 64 characters long
    if (rt_len < 64) rds_params.rt[rt_len] = '\r';
}

void set_rds_ps(char *ps) {
    rds_controls.ps_update = 1;
    strncpy(rds_params.ps, ps, 8);
    for(int i=0; i<8; i++) {
        if(rds_params.ps[i] == 0) rds_params.ps[i] = 32;
    }
}

void set_rds_rtp_flags(int running, int toggle) {
    rtp_params.running = running;
    rtp_params.toggle = toggle;
}

void set_rds_rtp_tags(int type_1, int start_1, int len_1,
                      int type_2, int start_2, int len_2) {
    rtp_params.type_1 = type_1;
    rtp_params.start_1 = start_1;
    rtp_params.len_1 = len_1;
    rtp_params.type_2 = type_2;
    rtp_params.start_2 = start_2;
    rtp_params.len_2 = len_2;
}

void set_rds_af(int *af_array) {
    rds_params.af[0] = af_array[0];
    for(int f=1; f<af_array[0]+1; f++) {
        rds_params.af[f] = af_array[f];
    }
}

void set_rds_pty(int pty) {
    rds_params.pty = pty;
}

void set_rds_ptyn(char *ptyn, int enable) {
    rds_controls.enable_ptyn = enable;
    if (!rds_controls.enable_ptyn) return;

    rds_controls.ptyn_update = 1;
    strncpy(rds_params.ptyn, ptyn, 8);
}

void set_rds_ta(int ta) {
    rds_params.ta = ta;
}

void set_rds_tp(int tp) {
    rds_params.tp = tp;
}

void set_rds_ms(int ms) {
    rds_params.ms = ms;
}

void set_rds_ab(int ab) {
    rds_controls.ab = ab;
}
