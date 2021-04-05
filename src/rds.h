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

typedef struct {
	uint8_t num_afs;
	uint8_t af[MAX_AF];
} rds_af_t;

#define DI_STEREO	1 // 1 - Stereo
#define DI_AH		2 // 2 - Artificial Head
#define DI_COMPRESSED	4 // 4 - Compressed
#define DI_DPTY		8 // 8 - Dynamic PTY

extern int init_rds_encoder(uint16_t pi, char *ps, char *rt, uint8_t pty,
			    uint8_t tp, rds_af_t init_afs, char *ptyn,
			    char *call_sign);

extern void add_checkwords(uint16_t *blocks, uint8_t *bits);
extern float get_rds_sample();

extern void set_rds_pi(uint16_t pi_code);
extern void set_rds_rt(char *rt);
extern void set_rds_ps(char *ps);
extern void set_rds_rtp_flags(uint8_t running, uint8_t toggle);
extern void set_rds_rtp_tags(uint8_t type_1, uint8_t start_1, uint8_t len_1,
			     uint8_t type_2, uint8_t start_2, uint8_t len_2);
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
