/*
    PiFmAdv - Advanced FM transmitter for the Raspberry Pi
    Copyright (C) 2017 Miegl

    See https://github.com/Miegl/PiFmAdv
*/

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "waveforms.h"

#define GROUP_LENGTH 4

struct {
    uint16_t pi;
    int ta;
    int pty;
    int tp;
    int ms;
    int ab;
    char ps[8];
    char ps_dynamic[8];
    int ps_update;
    char rt[64];
    char rt_dynamic[64];
    int rt_update;
    int af[100];
    int enable_ptyn;
    char ptyn[8];
    char ptyn_dynamic[8];
    int ptyn_update;
    // RT+
    int rt_p_toggle;
    int rt_p_running;
    int rt_p_type_1;
    int rt_p_start_1;
    int rt_p_len_1;
    int rt_p_type_2;
    int rt_p_start_2;
    int rt_p_len_2;
} rds_params = { 0 };
/* Here, the first member of the struct must be a scalar to avoid a
   warning on -Wmissing-braces with GCC < 4.8.3
   (bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119)
*/

/* The RDS error-detection code generator polynomial is
   x^10 + x^8 + x^7 + x^5 + x^4 + x^3 + x^0
*/
#define POLY 0x1B9
#define POLY_DEG 10
#define MSB_BIT 0x8000
#define BLOCK_SIZE 16

#define BITS_PER_GROUP (GROUP_LENGTH * (BLOCK_SIZE+POLY_DEG))
#define SAMPLES_PER_BIT 192
#define FILTER_SIZE (sizeof(waveform_biphase)/sizeof(float))
#define SAMPLE_BUFFER_SIZE (SAMPLES_PER_BIT + FILTER_SIZE)

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
            crc = crc ^ POLY;
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
    struct tm *utc;

    now = time (NULL);
    utc = gmtime (&now);

    if(utc->tm_min != latest_minutes) {
        // Generate CT group
        latest_minutes = utc->tm_min;

        int l = utc->tm_mon <= 1 ? 1 : 0;
        int mjd = 14956 + utc->tm_mday +
                  (int)((utc->tm_year - l) * 365.25) +
                  (int)((utc->tm_mon + 2 + l*12) * 30.6001);

        blocks[1] |= 0x4000 | (mjd>>15);
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
int get_rds_ps_group(uint16_t *blocks) {
	static int ps_state, af_state;

	blocks[1] |= 0x0000 | rds_params.ta << 4 | rds_params.ms << 3 | ps_state;
	if (ps_state == 3) blocks[1] |= 0x0004; // DI = 1 - Stereo
	if (rds_params.af[0]) { // AF
		if (af_state == 0) {
			blocks[2] = (rds_params.af[0] + 224) << 8 | rds_params.af[1];
		} else {
			blocks[2] = (rds_params.af[af_state+1]) ?
				rds_params.af[af_state] << 8 | rds_params.af[af_state+1] :
				rds_params.af[af_state] << 8 | 0xCD;
		}
		af_state = af_state + 2;
		if (af_state > rds_params.af[0]) af_state = 0;
	}
	blocks[3] = rds_params.ps[ps_state*2] << 8 | rds_params.ps[ps_state*2+1];
	ps_state++;
	if (ps_state == 4) {
		ps_state = 0;
		if (rds_params.ps_update) {
			strncpy(rds_params.ps, rds_params.ps_dynamic, 8);
			rds_params.ps_update = 0;
		}
	}
	return 1;
}

/* RT group (2A)
 */
int get_rds_rt_group(uint16_t *blocks) {
	static int rt_state;

restart_rt:
	if (rds_params.rt_update) {
		strncpy(rds_params.rt, rds_params.rt_dynamic, 64);
		rds_params.ab ^= 1;
		rds_params.rt_update = 0;
		rt_state = 0;
	}

	if ((rds_params.rt[rt_state*4+0] << 8 | rds_params.rt[rt_state*4+1] |
	     rds_params.rt[rt_state*4+2] << 8 | rds_params.rt[rt_state*4+3]) == 0) {
		rt_state = 0;
		goto restart_rt;
	}

	blocks[1] |= 0x2000 | rds_params.ab << 4 | rt_state;
	blocks[2] = rds_params.rt[rt_state*4+0] << 8 | rds_params.rt[rt_state*4+1];
	blocks[3] = rds_params.rt[rt_state*4+2] << 8 | rds_params.rt[rt_state*4+3];

	rt_state++;
	if (rt_state == 16) rt_state = 0;
	return 1;
}

/* ODA group (3A)
 */
int get_rds_oda_group(uint16_t *blocks) {
	static int oda_state;

	switch (oda_state) {
	case 0: // RT+
		// Assign the RT+ AID to group 11A
		blocks[1] |= 0x3000 | 11 << 1 | 0;
		blocks[3] = 0x4BD7; // RT+ AID
		break;
	}

	oda_state++;
	if (oda_state == 1) oda_state = 0;
	return 1;
}

/* PTYN group (10A)
 */
int get_rds_ptyn_group(uint16_t *blocks) {
	static int ptyn_state;

	blocks[1] |= 0xA000 | ptyn_state;
	blocks[2] = rds_params.ptyn[ptyn_state*4+0] << 8 | rds_params.ptyn[ptyn_state*4+1];
	blocks[3] = rds_params.ptyn[ptyn_state*4+2] << 8 | rds_params.ptyn[ptyn_state*4+3];
	ptyn_state++;
	if (ptyn_state == 2) {
		ptyn_state = 0;
		if (rds_params.ptyn_update) {
			strncpy(rds_params.ptyn, rds_params.ptyn_dynamic, 8);
			rds_params.ptyn_update = 0;
		}
	}
	return 1;
}

/* RT+ group (assigned to 11A)
 */
int get_rds_rtp_group(uint16_t *blocks) {
	// RT+ block format
	blocks[1] |= 0xB000 | rds_params.rt_p_toggle << 4 | rds_params.rt_p_running << 3 |
		    (rds_params.rt_p_type_1 & 0x38) >> 3;
	blocks[2] = (rds_params.rt_p_type_1 & 0x7) << 13 | (rds_params.rt_p_start_1 & 0x3F) << 7 |
		    (rds_params.rt_p_len_1 & 0x3F) << 1 | (rds_params.rt_p_type_2 & 0x20) >> 5;
	blocks[3] = (rds_params.rt_p_type_2 & 0x1F) << 11 | (rds_params.rt_p_start_2 & 0x3F) << 5 |
		    (rds_params.rt_p_len_2 & 0x1F);
	return 1;
}

/* Other RDS groups have lower priority than 0A and 2A and are
   therefore placed in a subsequence
 */
int get_rds_other_groups(uint16_t *blocks) {
	static int state;

skip_group:
	if (state == 3) state = 0;

	switch (state) {
	case 0: // Type 3A groups
		get_rds_oda_group(blocks);
		break;
	case 1: // Type 10A groups
		if (!rds_params.enable_ptyn) { // Do not generate a 10A group if PTYN is off
			state++;
			goto skip_group;
		} else {
			get_rds_ptyn_group(blocks);
		}
		break;
	case 2: // Type 11A groups
		get_rds_rtp_group(blocks);
		break;
	}

	state++;
	return 1;
}

/* Creates an RDS group. This generates sequences of the form 0A, 2A, 0A, 2A, 0A, 2A, 0A, 2A, etc.
   The pattern is of length 8, the variable 'state' keeps track of where we are in the
   pattern. 'ps_state' and 'rt_state' keep track of where we are in the PS (0A) sequence
   or RT (2A) sequence, respectively.
*/
void get_rds_group(int *buffer) {
    static int state;
    // Basic block data
    uint16_t blocks[GROUP_LENGTH] = {rds_params.pi, rds_params.tp << 10 | rds_params.pty << 5, 0, 0};

    // Generate block content
    if(!get_rds_ct_group(blocks)) { // CT (clock time) has priority on other group types
	switch (state) {
	case 0:
	case 2:
	case 4: // Type 0A groups
	    get_rds_ps_group(blocks);
	    break;
	case 1:
	case 3: // Type 2A groups
	    get_rds_rt_group(blocks);
	    break;
	case 5: // Other groups
	    get_rds_other_groups(blocks);
	    break;
	}

	state++;
	if(state == 6) state = 0;
    }

    // Calculate the checkword for each block and emit the bits
    for(int i=0; i<GROUP_LENGTH; i++) {
        uint16_t block = blocks[i];
        uint16_t check = crc(block) ^ offset_words[i];
        for(int j=0; j<BLOCK_SIZE; j++) {
            *buffer++ = ((block & (1<<(BLOCK_SIZE-1))) != 0);
            block <<= 1;
        }
        for(int j=0; j<POLY_DEG; j++) {
            *buffer++= ((check & (1<<(POLY_DEG-1))) != 0);
            check <<= 1;
        }
    }
}

/* Get a number of RDS samples. This generates the envelope of the waveform using
   pre-generated elementary waveform samples, and then it amplitude-modulates the
   envelope with a 57 kHz carrier, which is very efficient as 57 kHz is 4 times the
   sample frequency we are working at (228 kHz).
 */
void get_rds_samples(float *buffer, int count) {
    static int bit_buffer[BITS_PER_GROUP];
    static int bit_pos = BITS_PER_GROUP;
    static float sample_buffer[SAMPLE_BUFFER_SIZE] = {0};

    static int prev_output = 0;
    static int cur_output = 0;
    static int cur_bit = 0;
    static int sample_count = SAMPLES_PER_BIT;
    static int inverting = 0;
    static int phase = 0;

    static int in_sample_index = 0;
    static int out_sample_index = SAMPLE_BUFFER_SIZE-1;

    for(int i=0; i<count; i++) {
        if(sample_count >= SAMPLES_PER_BIT) {
            if(bit_pos >= BITS_PER_GROUP) {
                get_rds_group(bit_buffer);
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
                if(idx >= SAMPLE_BUFFER_SIZE) idx = 0;
            }

            in_sample_index += SAMPLES_PER_BIT;
            if(in_sample_index >= SAMPLE_BUFFER_SIZE) in_sample_index -= SAMPLE_BUFFER_SIZE;

            bit_pos++;
            sample_count = 0;
        }

        float sample = sample_buffer[out_sample_index];
        sample_buffer[out_sample_index] = 0;
        out_sample_index++;
        if(out_sample_index >= SAMPLE_BUFFER_SIZE) out_sample_index = 0;


        // modulate at 57 kHz
        // use phase for this
        switch(phase) {
            case 0:
            case 2: sample = 0; break;
            case 1: break;
            case 3: sample = -sample; break;
        }
        phase++;
        if(phase >= 4) phase = 0;

        *buffer++ = sample;
        sample_count++;
    }
}

void set_rds_pi(uint16_t pi_code) {
    rds_params.pi = pi_code;
}

void set_rds_rt(char *rt) {
    strncpy(rds_params.rt, rt, 64);
    int rt_len = strlen(rt);
    // Terminate RT with '\r' (carriage return) if RT is < 64 characters long
    if (rt_len < 64)
	rds_params.rt[rt_len] = '\r';
}

void set_rds_rt_dynamic(char *rt) {
    strncpy(rds_params.rt_dynamic, rt, 64);
    int rt_len = strlen(rt);
    if (rt_len < 64)
	rds_params.rt_dynamic[rt_len] = '\r';
    rds_params.rt_update = 1;
}

void set_rds_ps(char *ps) {
    strncpy(rds_params.ps, ps, 8);
    for(int i=0; i<8; i++) {
        if(rds_params.ps[i] == 0) rds_params.ps[i] = 32;
    }
}

void set_rds_ps_dynamic(char *ps) {
    strncpy(rds_params.ps_dynamic, ps, 8);
    for(int i=0; i<8; i++)
        if(rds_params.ps_dynamic[i] == 0) rds_params.ps_dynamic[i] = 32;
    rds_params.ps_update = 1;
}

void set_rds_rtp_flags(int rt_p_toggle, int rt_p_running) {
    rds_params.rt_p_toggle = rt_p_toggle;
    rds_params.rt_p_running = rt_p_running;
}

void set_rds_rtp_tags(int rt_p_type_1, int rt_p_start_1, int rt_p_len_1,
                      int rt_p_type_2, int rt_p_start_2, int rt_p_len_2) {
    rds_params.rt_p_type_1 = rt_p_type_1;
    rds_params.rt_p_start_1 = rt_p_start_1;
    rds_params.rt_p_len_1 = rt_p_len_1;
    rds_params.rt_p_type_2 = rt_p_type_2;
    rds_params.rt_p_start_2 = rt_p_start_2;
    rds_params.rt_p_len_2 = rt_p_len_2;
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

void set_rds_ptyn_enable(int enable_ptyn) {
    rds_params.enable_ptyn = enable_ptyn;
}

void set_rds_ptyn(char *ptyn) {
    strncpy(rds_params.ptyn, ptyn, 8);
}

void set_rds_ptyn_dynamic(char *ptyn) {
    strncpy(rds_params.ptyn_dynamic, ptyn, 8);
    rds_params.ptyn_update = 1;
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
    rds_params.ab = ab;
}
