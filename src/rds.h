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

#ifndef RDS_H
#define RDS_H

#include <stdint.h>

/* The RDS error-detection code generator polynomial is
   x^10 + x^8 + x^7 + x^5 + x^4 + x^3 + x^0
*/
#define POLY 0x1B9
#define POLY_DEG 10
#define MSB_BIT 0x8000
#define BLOCK_SIZE 16

#define GROUP_LENGTH 4
#define BITS_PER_GROUP (GROUP_LENGTH * (BLOCK_SIZE+POLY_DEG))
#define SAMPLES_PER_BIT 160
#define WAVEFORM_SIZE 1120
#define SAMPLE_BUFFER_SIZE (SAMPLES_PER_BIT + WAVEFORM_SIZE)

#define MAX_AF 25

typedef struct rds_af_t {
	uint8_t num_afs;
	uint8_t af[MAX_AF];
} rds_af_t;

typedef struct rds_params_t {
	uint16_t pi;
	uint8_t ta;
	uint8_t pty;
	uint8_t tp;
	uint8_t ms;
	uint8_t di;
	// PS
	char ps[8];
	// RT
	char rt[64];
	// PTYN
	char ptyn[8];

	rds_af_t af;

	uint8_t tx_ctime;
} rds_params_t;
/* Here, the first member of the struct must be a scalar to avoid a
   warning on -Wmissing-braces with GCC < 4.8.3
   (bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119)
*/

/* Group type
 *
 * 0-15
 */
#define GROUP_TYPE_0	(0 << 4)
#define GROUP_TYPE_1	(1 << 4)
#define GROUP_TYPE_2	(2 << 4)
#define GROUP_TYPE_3	(3 << 4)
#define GROUP_TYPE_4	(4 << 4)
#define GROUP_TYPE_5	(5 << 4)
#define GROUP_TYPE_6	(6 << 4)
#define GROUP_TYPE_7	(7 << 4)
#define GROUP_TYPE_8	(8 << 4)
#define GROUP_TYPE_9	(9 << 4)
#define GROUP_TYPE_10	(10 << 4)
#define GROUP_TYPE_11	(11 << 4)
#define GROUP_TYPE_12	(12 << 4)
#define GROUP_TYPE_13	(13 << 4)
#define GROUP_TYPE_14	(14 << 4)
#define GROUP_TYPE_15	(15 << 4)

/* Group versions
 *
 * The first 4 bits are the group number and the remaining 4 are
 * the group version
 */
#define GROUP_VER_A	0
#define GROUP_VER_B	1

// Version A groups
#define GROUP_0A	(GROUP_TYPE_0 | GROUP_VER_A)
#define GROUP_1A	(GROUP_TYPE_1 | GROUP_VER_A)
#define GROUP_2A	(GROUP_TYPE_2 | GROUP_VER_A)
#define GROUP_3A	(GROUP_TYPE_3 | GROUP_VER_A)
#define GROUP_4A	(GROUP_TYPE_4 | GROUP_VER_A)
#define GROUP_5A	(GROUP_TYPE_5 | GROUP_VER_A)
#define GROUP_6A	(GROUP_TYPE_6 | GROUP_VER_A)
#define GROUP_7A	(GROUP_TYPE_7 | GROUP_VER_A)
#define GROUP_8A	(GROUP_TYPE_8 | GROUP_VER_A)
#define GROUP_9A	(GROUP_TYPE_9 | GROUP_VER_A)
#define GROUP_10A	(GROUP_TYPE_10 | GROUP_VER_A)
#define GROUP_11A	(GROUP_TYPE_11 | GROUP_VER_A)
#define GROUP_12A	(GROUP_TYPE_12 | GROUP_VER_A)
#define GROUP_13A	(GROUP_TYPE_13 | GROUP_VER_A)
#define GROUP_14A	(GROUP_TYPE_14 | GROUP_VER_A)
#define GROUP_15A	(GROUP_TYPE_15 | GROUP_VER_A)

// Version B groups
#define GROUP_0B	(GROUP_TYPE_0 | GROUP_VER_B)
#define GROUP_1B	(GROUP_TYPE_1 | GROUP_VER_B)
#define GROUP_2B	(GROUP_TYPE_2 | GROUP_VER_B)
#define GROUP_3B	(GROUP_TYPE_3 | GROUP_VER_B)
#define GROUP_4B	(GROUP_TYPE_4 | GROUP_VER_B)
#define GROUP_5B	(GROUP_TYPE_5 | GROUP_VER_B)
#define GROUP_6B	(GROUP_TYPE_6 | GROUP_VER_B)
#define GROUP_7B	(GROUP_TYPE_7 | GROUP_VER_B)
#define GROUP_8B	(GROUP_TYPE_8 | GROUP_VER_B)
#define GROUP_9B	(GROUP_TYPE_9 | GROUP_VER_B)
#define GROUP_10B	(GROUP_TYPE_10 | GROUP_VER_B)
#define GROUP_11B	(GROUP_TYPE_11 | GROUP_VER_B)
#define GROUP_12B	(GROUP_TYPE_12 | GROUP_VER_B)
#define GROUP_13B	(GROUP_TYPE_13 | GROUP_VER_B)
#define GROUP_14B	(GROUP_TYPE_14 | GROUP_VER_B)
#define GROUP_15B	(GROUP_TYPE_15 | GROUP_VER_B)

#define GET_GROUP_TYPE(x)	((x >> 4) & 15)
#define GET_GROUP_VER(x)	(x & 1) // only check bit 0

/* RDS ODA ID group
 *
 * This struct is for defining ODAs that will be transmitted
 *
 * Can signal version A or B data groups
 */
typedef struct rds_oda_t {
	uint8_t group;
	uint16_t aid;
	uint16_t scb;
} rds_oda_t;

#define DI_STEREO	1 // 1 - Stereo
#define DI_AH		2 // 2 - Artificial Head
#define DI_COMPRESSED	4 // 4 - Compressed
#define DI_DPTY		8 // 8 - Dynamic PTY

extern int init_rds_encoder(rds_params_t rds_params, char *call_sign);
extern void add_checkwords(uint16_t *blocks, uint8_t *bits);
extern float get_rds_sample();
extern void set_rds_pi(uint16_t pi_code);
extern void set_rds_rt(char *rt);
extern void set_rds_ps(char *ps);
extern void set_rds_rtplus_flags(uint8_t running, uint8_t toggle);
extern void set_rds_rtplus_tags(uint8_t *tags);
extern void set_rds_ta(uint8_t ta);
extern void set_rds_pty(uint8_t pty);
extern void set_rds_ptyn(char *ptyn);
extern void set_rds_af(rds_af_t new_af_list);
extern void set_rds_tp(uint8_t tp);
extern void set_rds_ms(uint8_t ms);
extern void set_rds_ab(uint8_t ab);
extern void set_rds_ct(uint8_t ct);
extern void set_rds_di(uint8_t di);

#endif /* RDS_H */
