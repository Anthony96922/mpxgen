/*
    mpxgen - FM multiplex encoder with Stereo and RDS
    Copyright (C) 2019 Anthony96922

    See https://github.com/Anthony96922/mpxgen
*/

extern int fm_mpx_open(char *filename, size_t len, int preemphasis);
extern int fm_mpx_get_samples(float *mpx_buffer, float *rds_buffer, int rds, int wait);
extern int fm_mpx_close();
