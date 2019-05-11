/*
    PiFmAdv - Advanced FM transmitter for the Raspberry Pi
    Copyright (C) 2017 Miegl

    See https://github.com/Miegl/PiFmAdv
*/

extern int fm_mpx_open(char *filename, size_t len, int preemphasis_corner_freq);
extern int fm_mpx_get_samples(double *mpx_buffer, double *rds_buffer, int rds, int wait);
extern int fm_mpx_close();
