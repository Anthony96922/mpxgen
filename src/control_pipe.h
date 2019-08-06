/*
    mpxgen - FM multiplex encoder with Stereo and RDS
    Copyright (C) 2019 Anthony96922

    See https://github.com/Anthony96922/mpxgen
*/

#define CONTROL_PIPE_PS_SET 1
#define CONTROL_PIPE_RT_SET 2
#define CONTROL_PIPE_TA_SET 3
#define CONTROL_PIPE_PTY_SET 4
#define CONTROL_PIPE_TP_SET 5
#define CONTROL_PIPE_MS_SET 6
#define CONTROL_PIPE_AB_SET 7
#define CONTROL_PIPE_RTP_SET 8
#define CONTROL_PIPE_RTP_FLAGS_SET 9
#define CONTROL_PIPE_PTYN_SET 10

extern int open_control_pipe(char *filename);
extern int close_control_pipe();
extern int poll_control_pipe();
