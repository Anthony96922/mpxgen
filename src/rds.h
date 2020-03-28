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
#define SAMPLES_PER_BIT 192
#define FILTER_SIZE 576
#define SAMPLE_BUFFER_SIZE (SAMPLES_PER_BIT + FILTER_SIZE)

#define MAX_AF 25

extern void rds_encoder_init(uint16_t pi, char *ps, char *rt, int pty,
                             int tp, int *af_array, char *ptyn);

extern float get_rds_sample();

extern void set_rds_pi(uint16_t pi_code);
extern void set_rds_rt(char *rt);
extern void set_rds_ps(char *ps);
extern void set_rds_rtp_flags(int running, int toggle);
extern void set_rds_rtp_tags(int type_1, int start_1, int len_1,
                             int type_2, int start_2, int len_2);
extern void set_rds_ta(int ta);
extern void set_rds_pty(int pty);
extern void set_rds_ptyn(char *ptyn, int enable);
extern void set_rds_af(int *af_array);
extern void set_rds_tp(int tp);
extern void set_rds_ms(int ms);
extern void set_rds_ab(int ab);
extern void set_rds_ct(int ct);
extern void set_rds_di(int di);

#endif /* RDS_H */
