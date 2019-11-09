/*
    mpxgen - FM multiplex encoder with Stereo and RDS
    Copyright (C) 2019 Anthony96922

    See https://github.com/Anthony96922/mpxgen
*/

extern int fm_mpx_open(char *filename, size_t len, int rds_on);
extern int fm_mpx_get_samples(float *mpx_buffer);
extern int fm_mpx_close();
extern int channels;
extern void set_19_level(int level);
extern void set_38_level(int level);
extern void set_57_level(int level);
