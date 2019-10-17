/*
    mpxgen - FM multiplex encoder with Stereo and RDS
    Copyright (C) 2019 Anthony96922

    See https://github.com/Anthony96922/mpxgen
*/

#define DATA_SIZE 4096

extern int fm_mpx_open(char *filename, size_t len, int rds_on, int wait_for_audio);
extern int fm_mpx_get_samples(float *mpx_buffer);
extern int fm_mpx_close();
extern int channels;
