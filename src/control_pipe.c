/*
    mpxgen - FM multiplex encoder with Stereo and RDS
    Copyright (C) 2019 Anthony96922

    See https://github.com/Anthony96922/mpxgen
*/

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

#include "rds.h"
#include "control_pipe.h"

#define CTL_BUFFER_SIZE 100

FILE *f_ctl;

/*
 * Opens a file (pipe) to be used to control the RDS coder, in non-blocking mode.
 */

int open_control_pipe(char *filename) {
	int fd = open(filename, O_RDONLY | O_NONBLOCK);
    if(fd == -1) return -1;

	int flags;
	flags = fcntl(fd, F_GETFL, 0);
	flags |= O_NONBLOCK;
	if( fcntl(fd, F_SETFL, flags) == -1 ) return -1;

	f_ctl = fdopen(fd, "r");
	if(f_ctl == NULL) return -1;

	return 0;
}


/*
 * Polls the control file (pipe), non-blockingly, and if a command is received,
 * processes it and updates the RDS data.
 */

int poll_control_pipe() {
	static char buf[CTL_BUFFER_SIZE];

    char *res = fgets(buf, CTL_BUFFER_SIZE, f_ctl);
    if(res == NULL) return -1;
    if(strlen(res) > 3 && res[2] == ' ') {
        char *arg = res+3;
        if(arg[strlen(arg)-1] == '\n') arg[strlen(arg)-1] = 0;
        if(res[0] == 'P' && res[1] == 'S') {
            arg[8] = 0;
            set_rds_ps(arg);
            printf("PS set to: \"%s\"\n", arg);
            return CONTROL_PIPE_PS_SET;
        }
        if(res[0] == 'R' && res[1] == 'T') {
            arg[64] = 0;
            set_rds_rt(arg);
            printf("RT set to: \"%s\"\n", arg);
            return CONTROL_PIPE_RT_SET;
        }
        if(res[0] == 'T' && res[1] == 'A') {
            int ta = ( strcmp(arg, "ON") == 0 );
            set_rds_ta(ta);
            printf("Set TA to ");
            if(ta) printf("ON\n"); else printf("OFF\n");
            return CONTROL_PIPE_TA_SET;
        }
	if(res[0] == 'T' && res[1] == 'P') {
            int tp = ( strcmp(arg, "ON") == 0 );
            set_rds_tp(tp);
            printf("Set TP to ");
            if(tp) printf("ON\n"); else printf("OFF\n");
            return CONTROL_PIPE_TP_SET;
        }
	if(res[0] == 'M' && res[1] == 'S') {
            int ms = ( strcmp(arg, "ON") == 0 );
            set_rds_ms(ms);
            printf("Set MS to ");
            if(ms) printf("ON\n"); else printf("OFF\n");
            return CONTROL_PIPE_MS_SET;
        }
	if(res[0] == 'A' && res[1] == 'B') {
            int ab = ( strcmp(arg, "ON") == 0 );
            set_rds_ab(ab);
            printf("Set AB to ");
            if(ab) printf("ON\n"); else printf("OFF\n");
            return CONTROL_PIPE_AB_SET;
        }
    }

    if(strlen(res) > 4 && res[3] == ' ') {
        char *arg = res+4;
        if(arg[strlen(arg)-1] == '\n') arg[strlen(arg)-1] = 0;
        if(res[0] == 'P' && res[1] == 'T' && res[2] == 'Y') {
            int pty = atoi(arg);
            if (pty >= 0 && pty <= 31) {
                set_rds_pty(pty);
                if (!pty) {
                    printf("PTY disabled\n");
                } else {
                    printf("PTY set to: %i\n", pty);
                }
            }
            else {
                printf("Wrong PTY identifier! The PTY range is 0 - 31.\n");
            }
            return CONTROL_PIPE_PTY_SET;
        }
        if (res[0] == 'R' && res[1] == 'T' && res[2] == 'P') {
            int type_1, start_1, len_1, type_2, start_2, len_2;
            if (sscanf(arg, "%u,%u,%u,%u,%u,%u", &type_1, &start_1, &len_1, &type_2, &start_2, &len_2) == 6) {
                if (type_1 > 63) type_1 = 0;
                if (type_2 > 63) type_2 = 0;
                if (start_1 > 64) start_1 = 0;
                if (start_2 > 64) start_2 = 0;
                if (len_1 > 64) len_1 = 1;
                if (len_2 > 32) len_2 = 1;
                printf("RT+ tag 1: type: %u, start: %u, length: %u\n", type_1, start_1, len_1);
                printf("RT+ tag 2: type: %u, start: %u, length: %u\n", type_2, start_2, len_2);
                set_rds_rtp_tags(type_1, start_1, len_1, type_2, start_2, len_2);
            } else {
                printf("Could not parse RT+ tag info.\n");
            }
            return CONTROL_PIPE_RTP_SET;
        }
    }
    if (strlen(res) > 5 && res[4] == ' ') {
        char *arg = res+5;
        if(arg[strlen(arg)-1] == '\n') arg[strlen(arg)-1] = 0;
	if (res[0] == 'R' && res[1] == 'T' && res[2] == 'P' && res[3] == 'F') {
            int toggle, running;
            if (sscanf(arg, "%u,%u", &toggle, &running) == 2) {
                if (toggle > 1) toggle = 0;
                if (running > 1) running = 0;
                printf("RT+ flags: toggle: %u, running: %u\n", toggle, running);
                set_rds_rtp_flags(toggle, running);
            } else {
                printf("Could not parse RT+ flags.\n");
            }
	    return CONTROL_PIPE_RTP_FLAGS_SET;
        }
        if (res[0] == 'P' && res[1] == 'T' && res[2] == 'Y' && res[3] == 'N') {
            arg[8] = 0;
            if (strcmp(arg, "OFF") == 0) {
                printf("PTYN disabled\n");
                set_rds_ptyn_enable(0);
            } else {
                printf("PTYN set to: \"%s\"\n", arg);
                set_rds_ptyn_enable(1);
                set_rds_ptyn(arg);
            }
            return CONTROL_PIPE_PTYN_SET;
        }
    }
    return -1;
}

int close_control_pipe() {
    if(f_ctl) return fclose(f_ctl);
    else return 0;
}
