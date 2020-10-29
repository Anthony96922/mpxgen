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
#include "rds.h"

static struct {
	uint16_t pi;
	uint8_t ta;
	uint8_t pty;
	uint8_t tp;
	uint8_t ms;
	uint8_t ab;
	uint8_t di;
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
static struct {
	uint8_t on;
	uint8_t ps_update;
	uint8_t rt_update;
	uint8_t rt_segments;
	uint8_t ptyn_update;
	uint8_t tx_ctime;
} rds_controls;

// AF
static struct {
	uint8_t num_afs;
	uint8_t afs[MAX_AF];
} af;

// RT+
static struct {
	uint8_t group_num;
	uint8_t running;
	uint8_t toggle;
	uint8_t type_1;
	uint8_t start_1;
	uint8_t len_1;
	uint8_t type_2;
	uint8_t start_2;
	uint8_t len_2;
} rtp_params;

static uint16_t offset_words[] = {0x0FC, 0x198, 0x168, 0x1B4};
// We don't handle offset word C' here for the sake of simplicity

/* Classical CRC computation */
static uint16_t crc(uint16_t block) {
	uint16_t crc = 0;

	for(int j=0; j<BLOCK_SIZE; j++) {
		int bit = (block & MSB_BIT) != 0;
		block <<= 1;

		int msb = (crc >> (POLY_DEG-1)) & 1;
		crc <<= 1;
		if((msb ^ bit) != 0) crc ^= POLY;
	}
	return crc;
}

// Calculate the checkword for each block and emit the bits
void add_checkwords(uint16_t *blocks, uint8_t *bits) {
	uint16_t block, check;
	for(int i=0; i<GROUP_LENGTH; i++) {
		block = blocks[i];
		check = crc(block) ^ offset_words[i];
		for(int j=0; j<BLOCK_SIZE; j++) {
			*bits++ = ((block & (1<<(BLOCK_SIZE-1))) != 0);
			block <<= 1;
		}
		for(int j=0; j<POLY_DEG; j++) {
			*bits++ = ((check & (1<<(POLY_DEG-1))) != 0);
			check <<= 1;
		}
	}
}

/* Generates a CT (clock time) group if the minute has just changed
 * Returns 1 if the CT group was generated, 0 otherwise
 */
static int get_rds_ct_group(uint16_t *blocks) {
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
static void get_rds_ps_group(uint16_t *blocks) {
	static char ps_text[8];
	static uint8_t ps_state, af_state;

	if (ps_state == 0 && rds_controls.ps_update) {
		strncpy(ps_text, rds_params.ps, 8);
		rds_controls.ps_update = 0;
	}

	blocks[1] |= /* 0 << 12 | */ rds_params.ta << 4 | rds_params.ms << 3 | ps_state;

	// DI
	blocks[1] |= ((rds_params.di >> (3 - ps_state)) & 1) << 2;

	// AF
	if (af.num_afs) {
		if (af_state == 0) {
			blocks[2] = (af.num_afs + 224) << 8 | af.afs[0];
		} else {
			blocks[2] = af.afs[af_state] << 8 |
					(af.afs[af_state+1] ? af.afs[af_state+1] : 0xCD);
		}
		af_state += 2;
		if (af_state > af.num_afs) af_state = 0;
	} else {
		blocks[2] = 0xE0CD; // no AF
	}

	blocks[3] = ps_text[ps_state*2] << 8 | ps_text[ps_state*2+1];

	ps_state++;
	if (ps_state == 4) ps_state = 0;
}

/* RT group (2A)
 */
static void get_rds_rt_group(uint16_t *blocks) {
	static char rt_text[64];
	static uint8_t rt_state;

	if (rds_controls.rt_update) {
		strncpy(rt_text, rds_params.rt, 64);
		rds_params.ab ^= 1;
		rds_controls.rt_update = 0;
		rt_state = 0; // rewind when new RT arrives
	}

	blocks[1] |= 2 << 12 | rds_params.ab << 4 | rt_state;
	blocks[2] = rt_text[rt_state*4+0] << 8 | rt_text[rt_state*4+1];
	blocks[3] = rt_text[rt_state*4+2] << 8 | rt_text[rt_state*4+3];

	rt_state++;
	if (rt_state == rds_controls.rt_segments) rt_state = 0;
}

/* ODA group (3A)
 */
static void get_rds_oda_group(uint16_t *blocks) {
	blocks[1] |= 3 << 12;

	// RT+
	blocks[1] |= rtp_params.group_num << 1;
	blocks[3] = 0x4BD7; // RT+ AID
}

/* PTYN group (10A)
 */
static void get_rds_ptyn_group(uint16_t *blocks) {
	static char ptyn_text[8];
	static uint8_t ptyn_state;

	if (ptyn_state == 0 && rds_controls.ptyn_update) {
		strncpy(ptyn_text, rds_params.ptyn, 8);
		rds_controls.ptyn_update = 0;
	}

	blocks[1] |= 10 << 12 | ptyn_state;
	blocks[2] = ptyn_text[ptyn_state*4+0] << 8 | ptyn_text[ptyn_state*4+1];
	blocks[3] = ptyn_text[ptyn_state*4+2] << 8 | ptyn_text[ptyn_state*4+3];

	ptyn_state++;
	if (ptyn_state == 2) ptyn_state = 0;
}

/* RT+ group
 */
static void get_rds_rtp_group(uint16_t *blocks) {
	// RT+ block format
	blocks[1] |= rtp_params.group_num << 12 | rtp_params.toggle << 4 |
		     rtp_params.running << 3 | (rtp_params.type_1 & 0x38) >> 3;
	blocks[2] = (rtp_params.type_1 & 0x7) << 13 | (rtp_params.start_1 & 0x3F) << 7 |
		    (rtp_params.len_1 & 0x3F) << 1 | (rtp_params.type_2 & 0x20) >> 5;
	blocks[3] = (rtp_params.type_2 & 0x1F) << 11 | (rtp_params.start_2 & 0x3F) << 5 |
		    (rtp_params.len_2 & 0x1F);
}

/* Lower priority groups are placed in a subsequence
 */
static int get_rds_other_groups(uint16_t *blocks) {
	static uint8_t group[15];
	uint8_t group_coded = 0;

	// Type 3A groups
	if (++group[3] == 20) {
		group[3] = 0;
		get_rds_oda_group(blocks);
		group_coded = 1;
	}

	// Type 10A groups
	if (!group_coded && ++group[10] == 10) {
		group[10] = 0;
		if (rds_params.ptyn[0]) {
			// Do not generate a 10A group if PTYN is off
			get_rds_ptyn_group(blocks);
			group_coded = 1;
		}
	}

	// Type 11A groups
	if (!group_coded && ++group[rtp_params.group_num] == 20) {
		group[rtp_params.group_num] = 0;
		get_rds_rtp_group(blocks);
		group_coded = 1;
	}

	return group_coded;
}

/* Creates an RDS group.
 * This generates sequences of the form 0A, 2A, 0A, 2A, 0A, 2A, etc.
 */
static void get_rds_group(uint16_t *blocks) {
	static uint8_t state;

	// Basic block data
	blocks[0] = rds_params.pi;
	blocks[1] = rds_params.tp << 10 | rds_params.pty << 5;

	// Generate block content
	// CT (clock time) has priority on other group types
	if(!(rds_controls.tx_ctime && get_rds_ct_group(blocks))) {
		if (!get_rds_other_groups(blocks)) { // Other groups
			if (!state++) { // Type 0A groups
				get_rds_ps_group(blocks);
			} else { // Type 2A groups
				get_rds_rt_group(blocks);
			}
			if(state == 2) state = 0;
		}
	}
}

static void get_rds_bits(uint8_t *out_buffer) {
	uint16_t out_blocks[GROUP_LENGTH] = {0};
	get_rds_group(out_blocks);
	add_checkwords(out_blocks, out_buffer);
}

/* Get an RDS sample. This generates the envelope of the waveform using
 * pre-generated elementary waveform samples.
 */
float get_rds_sample() {
	static uint8_t bit_buffer[BITS_PER_GROUP];
	static uint8_t bit_pos;
	static float sample_buffer[SAMPLE_BUFFER_SIZE];

	static uint8_t prev_output;
	static uint8_t cur_output;
	static uint8_t cur_bit;
	static uint8_t sample_count;
	static uint8_t inverting;

	static uint16_t in_sample_index;
	static uint16_t out_sample_index;

	if (!rds_controls.on) return 0;

	if(sample_count == SAMPLES_PER_BIT) {
		if(bit_pos == BITS_PER_GROUP) {
			get_rds_bits(bit_buffer);
			bit_pos = 0;
		}

		// do differential encoding
		cur_bit = bit_buffer[bit_pos++];
		prev_output = cur_output;
		cur_output = prev_output ^ cur_bit;

		inverting = (cur_output == 1);

		int idx = in_sample_index;

		for(int j=0; j<WAVEFORM_SIZE; j++) {
			sample_buffer[idx++] +=
				(!inverting) ? waveform_biphase[j] : -waveform_biphase[j];
			if(idx == SAMPLE_BUFFER_SIZE) idx = 0;
		}

		in_sample_index += SAMPLES_PER_BIT;
		if(in_sample_index == SAMPLE_BUFFER_SIZE) in_sample_index = 0;

		sample_count = 0;
	}
	sample_count++;

	float sample = sample_buffer[out_sample_index];

	sample_buffer[out_sample_index++] = 0;
	if(out_sample_index >= SAMPLE_BUFFER_SIZE) out_sample_index = 0;

	return sample;
}

/*
 * PI code calculator
 *
 * Calculates the PI code from a station's callsign.
 *
 * See
 * https://www.nrscstandards.org/standards-and-guidelines/documents/standards/nrsc-4-b.pdf
 * for more information.
 *
 */
static uint16_t callsign2pi(char *callsign) {
	uint16_t pi_code = 0;

	if (callsign[0] == 'K' || callsign[0] == 'k') {
		pi_code += 4096;
	} else if (callsign[0] == 'W' || callsign[0] == 'w') {
		pi_code += 21672;
	} else {
		return 0;
	}

	pi_code +=
		// Change nibbles to base-26 decimal
		(callsign[1] - (callsign[1] >= 'a' ? 0x61 : 0x41)) * 676 +
		(callsign[2] - (callsign[2] >= 'a' ? 0x61 : 0x41)) * 26 +
		(callsign[3] - (callsign[3] >= 'a' ? 0x61 : 0x41));

	// Call letter exceptions
	if ((pi_code & 0x0F00) == 0) { // When 3rd char is 0
		pi_code = 0xA000 + ((pi_code & 0xF000) >> 4) + (pi_code & 0x00FF);
	}

	if ((pi_code & 0x00FF) == 0) { // When 1st & 2nd chars are 0
		pi_code = 0xAF00 + ((pi_code & 0xFF00) >> 8);
	}

	return pi_code;
}

int init_rds_encoder(uint16_t pi, char *ps, char *rt, uint8_t pty, uint8_t tp, uint8_t *af_array, char *ptyn, char *call_sign) {

	// RBDS PTY list
	char ptys[32][16] = {
		"None", "News", "Information", "Sports",
		"Talk", "Rock", "Classic rock", "Adult hits",
		"Soft rock" , "Top 40", "Country", "Oldies",
		"Soft music", "Nostalgia", "Jazz", "Classical",
		"R&B", "Soft R&B", "Language", "Religious music",
		"Religious talk", "Personality", "Public", "College",
		"Spanish talk", "Spanish music", "Hip-Hop", "Unassigned",
		"Unassigned", "Weather", "Emergency test", "Emergency"
	};


	if (pty > 31) {
		fprintf(stderr, "PTY must be between 0 - 31.\n");
		return -1;
	}

	if (call_sign[3]) {
		uint16_t new_pi;
		if ((new_pi = callsign2pi(call_sign))) {
			fprintf(stderr, "Calculated PI code from callsign '%s'.\n", call_sign);
			pi = new_pi;
		} else {
			fprintf(stderr, "Invalid callsign '%s'.\n", call_sign);
		}
	}

	if (rds_controls.on) {
		fprintf(stderr, "RDS Options:\n");
		fprintf(stderr, "PI: %04X, PS: \"%s\", PTY: %d (%s), TP: %d\n",
			pi, ps, pty, ptys[pty], tp);
		fprintf(stderr, "RT: \"%s\"\n", rt);
	}

	// AF
	if(af_array[0]) {
		set_rds_af(af_array);
		if (rds_controls.on) {
			fprintf(stderr, "AF: %d,", af_array[0]);
			for(int f = 1; f <= af_array[0]; f++) {
				fprintf(stderr, " %.1f", (af_array[f]+875)/10.0);
			}
			fprintf(stderr, "\n");
		}
	}

	set_rds_pi(pi);
	set_rds_ps(ps);
	set_rds_ab(1);
	set_rds_rt(rt);
	set_rds_pty(pty);
	if (ptyn[0]) {
		if (rds_controls.on) fprintf(stderr, "PTYN: \"%s\"\n", ptyn);
		set_rds_ptyn(ptyn);
	}
	set_rds_tp(tp);
	set_rds_ct(1);
	set_rds_ms(1);
	set_rds_di(DI_STEREO);

	// Assign the RT+ AID to group 11A
	rtp_params.group_num = 11;

	return 0;
}

void set_rds_pi(uint16_t pi_code) {
	rds_params.pi = pi_code;
}

void set_rds_rt(char *rt) {
	uint8_t rt_len = strlen(rt);
	rds_controls.rt_update = 1;
	memset(rds_params.rt, 0, 64);
	memcpy(rds_params.rt, rt, rt_len);

	if (rt_len < 64) {
		/* Terminate RT with '\r' (carriage return) if RT
		 * is < 64 characters long
		 */
		rds_params.rt[rt_len++] = '\r';

		for (int i = 0; i < 64; i += 4) {
			if (i >= rt_len) {
				rds_controls.rt_segments = i / 4;
				break;
			}
			// We have reached the end of the text string
		}
	} else {
		// Default to 16 if RT is 64 characters long
		rds_controls.rt_segments = 16;
	}
}

void set_rds_ps(char *ps) {
	rds_controls.ps_update = 1;
	memset(rds_params.ps, ' ', 8);
	memcpy(rds_params.ps, ps, strlen(ps));
}

void set_rds_rtp_flags(uint8_t running, uint8_t toggle) {
	rtp_params.running = running;
	rtp_params.toggle = toggle;
}

void set_rds_rtp_tags(uint8_t type_1, uint8_t start_1, uint8_t len_1,
		      uint8_t type_2, uint8_t start_2, uint8_t len_2) {
	rtp_params.type_1 = type_1;
	rtp_params.start_1 = start_1;
	rtp_params.len_1 = len_1;
	rtp_params.type_2 = type_2;
	rtp_params.start_2 = start_2;
	rtp_params.len_2 = len_2;
}

void set_rds_af(uint8_t *af_array) {
	af.num_afs = af_array[0];
	for(int f=1; f<=af_array[0]; f++) {
		af.afs[f] = af_array[f];
	}
}

void set_rds_pty(uint8_t pty) {
	rds_params.pty = pty;
}

void set_rds_ptyn(char *ptyn) {
	rds_controls.ptyn_update = 1;
	memset(rds_params.ptyn, ' ', 8);
	memcpy(rds_params.ptyn, ptyn, strlen(ptyn));
}

void set_rds_ta(uint8_t ta) {
	rds_params.ta = ta;
}

void set_rds_tp(uint8_t tp) {
	rds_params.tp = tp;
}

void set_rds_ms(uint8_t ms) {
	rds_params.ms = ms;
}

void set_rds_ab(uint8_t ab) {
	rds_params.ab = ab;
}

void set_rds_di(uint8_t di) {
	rds_params.di = di;
}

void set_rds_ct(uint8_t ct) {
	rds_controls.tx_ctime = ct;
}

void set_rds_switch(uint8_t on) {
	rds_controls.on = on;
}
