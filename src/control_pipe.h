/*
    mpxgen - FM multiplex encoder with Stereo and RDS
    Copyright (C) 2019 Anthony96922

    See https://github.com/Anthony96922/mpxgen
*/

extern int open_control_pipe(char *filename);
extern int close_control_pipe();
extern int poll_control_pipe();
