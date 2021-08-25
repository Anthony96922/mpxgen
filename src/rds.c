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

#include "common.h"
#include "rds.h"

#include <time.h>

static rds_params_t rds_data;

// RDS data controls
static struct {
	uint8_t ps_update;
	uint8_t rt_update;
	uint8_t ab;
	uint8_t rt_segments;
	uint8_t rt_bursting;
	uint8_t ptyn_update;
} rds_state;

// ODA
#define MAX_ODAS 8
static rds_oda_t odas[MAX_ODAS];
static struct {
	uint8_t current;
	uint8_t count;
} oda_state;

// RT+
static struct {
	uint8_t group;
	uint8_t running;
	uint8_t toggle;
	uint8_t type[2];
	uint8_t start[2];
	uint8_t len[2];
} rtplus_cfg;

static void register_oda(uint8_t group, uint16_t aid, uint16_t scb) {

	if (oda_state.count == MAX_ODAS) return; // can't accept more ODAs

	odas[oda_state.count].group = group;
	odas[oda_state.count].aid = aid;
	odas[oda_state.count].scb = scb;
	oda_state.count++;
}

static uint16_t offset_words[] = {
	0x0FC,
	0x198,
	0x168,
	0x1B4,
	0x350 /* now we do */
};
// We don't handle offset word C' here for the sake of simplicity

/* Classical CRC computation */
static uint16_t crc(uint16_t block) {
	uint16_t crc = 0;

	for (int j = 0; j < BLOCK_SIZE; j++) {
		uint8_t bit = (block & MSB_BIT) != 0;
		block <<= 1;

		uint8_t msb = (crc >> (POLY_DEG-1)) & 1;
		crc <<= 1;
		if ((msb ^ bit) != 0) crc ^= POLY;
	}
	return crc;
}

// Calculate the checkword for each block and emit the bits
void add_checkwords(uint16_t *blocks, uint8_t *bits) {
	uint16_t block, check, offset_word;
	for (int i = 0; i < GROUP_LENGTH; i++) {
		block = blocks[i];
		offset_word = offset_words[i];
		if (((blocks[1] >> 11) & 1) && i == 3) offset_word = offset_words[5];
		check = crc(block) ^ offset_word;
		for (int j = 0; j < BLOCK_SIZE; j++) {
			*bits++ = ((block & (1 << (BLOCK_SIZE - 1))) != 0);
			block <<= 1;
		}
		for (int j = 0; j < POLY_DEG; j++) {
			*bits++ = ((check & (1 << (POLY_DEG - 1))) != 0);
			check <<= 1;
		}
	}
}

/* Generates a CT (clock time) group if the minute has just changed
 * Returns 1 if the CT group was generated, 0 otherwise
 */
static uint8_t get_rds_ct_group(uint16_t *blocks) {
	static uint8_t latest_minutes;

	// Check time
	time_t now = time(NULL);
	struct tm *utc = gmtime(&now);

	if (utc->tm_min != latest_minutes) {
		// Generate CT group
		latest_minutes = utc->tm_min;

		uint8_t l = utc->tm_mon <= 1 ? 1 : 0;
		uint16_t mjd = 14956 + utc->tm_mday +
			(uint16_t)((utc->tm_year - l) * 365.25) +
			(uint16_t)((utc->tm_mon + 2 + l*12) * 30.6001);

		blocks[1] |= 4 << 12 | (mjd>>15);
		blocks[2] = (mjd<<1) | (utc->tm_hour>>4);
		blocks[3] = (utc->tm_hour & 0xF)<<12 | utc->tm_min<<6;

		struct tm *local = localtime(&now);

		int8_t offset = local->tm_hour - utc->tm_hour;
		blocks[3] |= abs(offset);
		if (offset < 0) blocks[3] |= (1 << 5);

		return 1;
	}

	return 0;
}

/* PS group (0A)
 */
static void get_rds_ps_group(uint16_t *blocks) {
	static char ps_text[8];
	static uint8_t ps_state, af_state;

	if (ps_state == 0 && rds_state.ps_update) {
		strncpy(ps_text, rds_data.ps, 8);
		rds_state.ps_update = 0;
	}

	blocks[1] |= /* 0 << 12 | */ rds_data.ta << 4 | rds_data.ms << 3 | ps_state;

	// DI
	blocks[1] |= ((rds_data.di >> (3 - ps_state)) & 1) << 2;

	// AF
	if (rds_data.af.num_afs) {
		if (af_state == 0) {
			blocks[2] = (rds_data.af.num_afs + 224) << 8 | rds_data.af.af[0];
		} else {
			blocks[2] = rds_data.af.af[af_state] << 8 |
					(rds_data.af.af[af_state+1] ? rds_data.af.af[af_state+1] : 0xCD);
		}
		af_state += 2;
		if (af_state > rds_data.af.num_afs) af_state = 0;
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

	if (rds_state.rt_bursting) rds_state.rt_bursting--;

	if (rds_state.rt_update) {
		strncpy(rt_text, rds_data.rt, 64);
		rds_state.ab ^= 1;
		rds_state.rt_update = 0;
		rt_state = 0; // rewind when new RT arrives
	}

	blocks[1] |= 2 << 12 | rds_state.ab << 4 | rt_state;
	blocks[2] = rt_text[rt_state*4+0] << 8 | rt_text[rt_state*4+1];
	blocks[3] = rt_text[rt_state*4+2] << 8 | rt_text[rt_state*4+3];

	rt_state++;
	if (rt_state == rds_state.rt_segments) rt_state = 0;
}

/* ODA group (3A)
 */
static void get_rds_oda_group(uint16_t *blocks) {
	blocks[1] |= 3 << 12;

	// select ODA
	rds_oda_t this_oda = odas[oda_state.current++];

	blocks[1] |= GET_GROUP_TYPE(this_oda.group) << 1 |
		     GET_GROUP_VER(this_oda.group);
	blocks[2] = this_oda.scb;
	blocks[3] = this_oda.aid;
	if (oda_state.current == oda_state.count) oda_state.current = 0;
}

/* PTYN group (10A)
 */
static void get_rds_ptyn_group(uint16_t *blocks) {
	static char ptyn_text[8];
	static uint8_t ptyn_state;

	if (ptyn_state == 0 && rds_state.ptyn_update) {
		strncpy(ptyn_text, rds_data.ptyn, 8);
		rds_state.ptyn_update = 0;
	}

	blocks[1] |= 10 << 12 | ptyn_state;
	blocks[2] = ptyn_text[ptyn_state*4+0] << 8 | ptyn_text[ptyn_state*4+1];
	blocks[3] = ptyn_text[ptyn_state*4+2] << 8 | ptyn_text[ptyn_state*4+3];

	ptyn_state++;
	if (ptyn_state == 2) ptyn_state = 0;
}

// RT+
static void init_rtplus(uint8_t group) {
	register_oda(group, 0x4BD7 /* RT+ AID */, 0);
	rtplus_cfg.group = group;
}

/* RT+ group
 */
static void get_rds_rtplus_group(uint16_t *blocks) {
	// RT+ block format
	blocks[1] |= GET_GROUP_TYPE(rtplus_cfg.group) << 12 |
		     GET_GROUP_VER(rtplus_cfg.group) << 11 |
		     rtplus_cfg.toggle << 4 | rtplus_cfg.running << 3 |
		    (rtplus_cfg.type[0]  & BIT_U5) >> 3;
	blocks[2] = (rtplus_cfg.type[0]  & BIT_L3) << 13 |
		    (rtplus_cfg.start[0] & BIT_L6) << 7 |
		    (rtplus_cfg.len[0]   & BIT_L6) << 1 |
		    (rtplus_cfg.type[1]  & BIT_U3) >> 5;
	blocks[3] = (rtplus_cfg.type[1]  & BIT_L5) << 11 |
		    (rtplus_cfg.start[1] & BIT_L6) << 5 |
		    (rtplus_cfg.len[1]   & BIT_L5);
}

/* Lower priority groups are placed in a subsequence
 */
static uint8_t get_rds_other_groups(uint16_t *blocks) {
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
		if (rds_data.ptyn[0]) {
			// Do not generate a 10A group if PTYN is off
			get_rds_ptyn_group(blocks);
			group_coded = 1;
		}
	}

	// Type 11A groups
	if (!group_coded && ++group[rtplus_cfg.group] == 20) {
		group[rtplus_cfg.group] = 0;
		get_rds_rtplus_group(blocks);
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
	blocks[0] = rds_data.pi;
	blocks[1] = rds_data.tp << 10 | rds_data.pty << 5;
	blocks[2] = 0;
	blocks[3] = 0;

	// Generate block content
	// CT (clock time) has priority on other group types
	if (!(rds_data.tx_ctime && get_rds_ct_group(blocks))) {
		if (!get_rds_other_groups(blocks)) { // Other groups
			// These are always transmitted
			if (!state) { // Type 0A groups
				get_rds_ps_group(blocks);
				state++;
			} else { // Type 2A groups
				get_rds_rt_group(blocks);
				if (!rds_state.rt_bursting) state++;
			}
			if (state == 2) state = 0;
		}
	}
}

void get_rds_bits(uint8_t *bits) {
	static uint16_t out_blocks[GROUP_LENGTH];
	get_rds_group(out_blocks);
	add_checkwords(out_blocks, bits);
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

int8_t init_rds_encoder(rds_params_t rds_params, char *call_sign) {

	// The RDS pty region. This determines which PTY list to use
	enum rds_pty_regions {
		REGION_FCC, // NRSC RBDS
		REGION_ROW  // Rest of the world
	} pty_region = REGION_FCC;

	// RDS PTY list
	char ptys[2][32][30] = {
		// RBDS
		{
			"None", "News", "Information", "Sports",
			"Talk", "Rock", "Classic rock", "Adult hits",
			"Soft rock" , "Top 40", "Country", "Oldies",
			"Soft music", "Nostalgia", "Jazz", "Classical",
			"R&B", "Soft R&B", "Language", "Religious music",
			"Religious talk", "Personality", "Public", "College",
			"Spanish talk", "Spanish music", "Hip-Hop", "Unassigned",
			"Unassigned", "Weather", "Emergency test", "Emergency"
		},
#if 0
		{
			"None", "News", "Current affairs", "Information",
			"Sport", "Education", "Drama", "Culture", "Science",
			"Varied", "Pop music", "Rock music", "Easy listening",
			"Light classical", "Serious classical", "Other music",
			"Weather", "Finance", "Children's programs",
			"Social affairs", "Religion", "Phone-in", "Travel",
			"Leisure", "Jazz music", "Country music",
			"National music", "Oldies music", "Folk music",
			"Documentary", "Alarm test", "Alarm"
		}
#endif
	};

	if (rds_data.pty > 31) {
		fprintf(stderr, "PTY must be between 0 - 31.\n");
		return -1;
	}

	if (call_sign[3]) {
		uint16_t new_pi;
		if ((new_pi = callsign2pi(call_sign))) {
			fprintf(stderr, "Calculated PI code from callsign '%s'.\n", call_sign);
			rds_params.pi = new_pi;
		} else {
			fprintf(stderr, "Invalid callsign '%s'.\n", call_sign);
		}
	}

	fprintf(stderr, "RDS Options:\n");
	fprintf(stderr, "PI: %04X, PS: \"%s\", PTY: %d (%s), TP: %d\n",
		rds_params.pi,
		rds_params.ps,
		rds_params.pty,
		ptys[pty_region][rds_params.pty],
		rds_params.tp);
	fprintf(stderr, "RT: \"%s\"\n", rds_params.rt);

	// AF
	if (rds_params.af.num_afs) {
		set_rds_af(rds_params.af);
		fprintf(stderr, "AF: %d,", rds_params.af.num_afs);
		for (int f = 0; f < rds_params.af.num_afs; f++) {
			fprintf(stderr, " %.1f", (rds_params.af.af[f]+875)/10.0);
		}
		fprintf(stderr, "\n");
	}

	set_rds_pi(rds_params.pi);
	set_rds_ps(rds_params.ps);
	set_rds_ab(1);
	set_rds_rt(rds_params.rt);
	set_rds_pty(rds_params.pty);
	if (rds_params.ptyn[0]) {
		fprintf(stderr, "PTYN: \"%s\"\n", rds_params.ptyn);
		set_rds_ptyn(rds_params.ptyn);
	}
	set_rds_tp(rds_params.tp);
	set_rds_ct(1);
	set_rds_ms(1);
	set_rds_di(DI_STEREO);

	// Assign the RT+ AID to group 11A
	init_rtplus(GROUP_11A);

	return 0;
}

void set_rds_pi(uint16_t pi_code) {
	rds_data.pi = pi_code;
}

void set_rds_rt(char *rt) {
	uint8_t rt_len = strlen(rt);
	rds_state.rt_update = 1;
	memset(rds_data.rt, 0, 64);
	memcpy(rds_data.rt, rt, rt_len);

	if (rt_len < 64) {
		/* Terminate RT with '\r' (carriage return) if RT
		 * is < 64 characters long
		 */
		rds_data.rt[rt_len++] = '\r';

		for (int i = 0; i <= 64; i += 4) {
			if (i >= rt_len) {
				rds_state.rt_segments = i / 4;
				break;
			}
			// We have reached the end of the text string
		}
	} else {
		// Default to 16 if RT is 64 characters long
		rds_state.rt_segments = 16;
	}

	rds_state.rt_bursting = rds_state.rt_segments;
}

void set_rds_ps(char *ps) {
	rds_state.ps_update = 1;
	memset(rds_data.ps, ' ', 8);
	memcpy(rds_data.ps, ps, strlen(ps));
}

void set_rds_rtplus_flags(uint8_t running, uint8_t toggle) {
	if (running > 1) running = 1;
	if (toggle > 1) toggle = 1;
	rtplus_cfg.running = running;
	rtplus_cfg.toggle = toggle;
}

void set_rds_rtplus_tags(uint8_t *tags) {
	rtplus_cfg.type[0]	= (tags[0] < 63) ? tags[0] : 0;
	rtplus_cfg.start[0]	= (tags[1] < 64) ? tags[1] : 0;
	rtplus_cfg.len[0]	= (tags[2] < 63) ? tags[2] : 0;
	rtplus_cfg.type[1]	= (tags[3] < 63) ? tags[3] : 0;
	rtplus_cfg.start[1]	= (tags[4] < 64) ? tags[4] : 0;
	rtplus_cfg.len[1]	= (tags[5] < 32) ? tags[5] : 0;
}

void set_rds_af(rds_af_t new_af_list) {
	memcpy(&rds_data.af, &new_af_list, sizeof(new_af_list));
}

void set_rds_pty(uint8_t pty) {
	rds_data.pty = pty;
}

void set_rds_ptyn(char *ptyn) {
	rds_state.ptyn_update = 1;
	if (ptyn[0]) {
		memset(rds_data.ptyn, ' ', 8);
		memcpy(rds_data.ptyn, ptyn, strlen(ptyn));
	} else {
		memset(rds_data.ptyn, 0, 8);
	}
}

void set_rds_ta(uint8_t ta) {
	rds_data.ta = ta;
}

void set_rds_tp(uint8_t tp) {
	rds_data.tp = tp;
}

void set_rds_ms(uint8_t ms) {
	rds_data.ms = ms;
}

void set_rds_ab(uint8_t ab) {
	rds_state.ab = ab;
}

void set_rds_di(uint8_t di) {
	rds_data.di = di;
}

void set_rds_ct(uint8_t ct) {
	rds_data.tx_ctime = ct;
}
