/*
    PiFmAdv - Advanced FM transmitter for the Raspberry Pi
    Copyright (C) 2017 Miegl

    See https://github.com/Miegl/PiFmAdv
*/

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

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
            set_rds_ps_dynamic(arg);
            printf("PS set to: \"%s\"\n", arg);
            return CONTROL_PIPE_PS_SET;
        }
        if(res[0] == 'R' && res[1] == 'T') {
            arg[64] = 0;
            set_rds_rt_dynamic(arg);
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
	    int tag, type, start, len;
	    if (sscanf(arg, "%d %d %d %d", &tag, &type, &start, &len)) {
		if (tag != 1 && tag != 2) tag = 1;
		if (type > 63) type = 0;
		if (start < 1 && start > 64) start = 1;
		if (len > 64) len = 0;
		printf("RT+ tags: tag: %d, type: %d, start: %d, length: %d\n", tag, type, start, len);
		set_rds_rtp_tags(tag, type, start, len);
	    } else {
		printf("Could not parse RT+ tag info.");
	    }
	    return CONTROL_PIPE_RTP_SET;
	}
    }
    if (strlen(res) > 5 && res[4] == ' ') {
	char *arg = res+5;
	if(arg[strlen(arg)-1] == '\n') arg[strlen(arg)-1] = 0;
	if (res[0] == 'R' && res[1] == 'T' && res[2] == 'P' && res[3] == 'F') {
	    int toggle, running;
	    if (sscanf(arg, "%d %d", &toggle, &running)) {
		if (toggle != 0 && toggle != 1) toggle = 0;
		if (running != 0 && running != 1) running = 0;
		printf("RT+ flags: toggle: %d, running: %d\n", toggle, running);
		set_rds_rtp_flags(toggle, running);
	    } else {
		printf("Could not parse RT+ flags.\n");
	    }
	}
	return CONTROL_PIPE_RTP_FLAGS_SET;
    }

    return -1;
}

int close_control_pipe() {
    if(f_ctl) return fclose(f_ctl);
    else return 0;
}
