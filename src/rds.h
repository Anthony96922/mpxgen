/*
    mpxgen - FM multiplex encoder with Stereo and RDS
    Copyright (C) 2019 Anthony96922

    See https://github.com/Anthony96922/mpxgen
*/

#ifndef RDS_H
#define RDS_H

#include <stdint.h>

extern void get_rds_samples(float *buffer, int count);
extern void set_rds_pi(uint16_t pi_code);
extern void set_rds_rt(char *rt);
extern void set_rds_ps(char *ps);
extern void set_rds_rtp_flags(int rt_p_running, int rt_p_toggle);
extern void set_rds_rtp_tags(int rt_p_type_1, int rt_p_start_1, int rt_p_len_1,
                             int rt_p_type_2, int rt_p_start_2, int rt_p_len_2);
extern void set_rds_ta(int ta);
extern void set_rds_pty(int pty);
extern void set_rds_ptyn(char *ptyn, int enable);
extern void set_rds_af(int *af_array);
extern void set_rds_tp(int tp);
extern void set_rds_ms(int ms);
extern void set_rds_ab(int ab);

extern void rds_encoder_init(uint16_t init_pi, char *init_ps,
                             char *init_rt, int init_pty, int init_tp);
#endif /* RDS_H */
