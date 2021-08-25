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

#include "common.h"
#include <fcntl.h>

#include "rds.h"
#include "fm_mpx.h"

//#define CONTROL_PIPE_MESSAGES

#define CTL_BUFFER_SIZE 100

static FILE *f_ctl;

/*
 * Opens a file (pipe) to be used to control the RDS coder, in non-blocking mode.
 */

int open_control_pipe(char *filename) {
	int fd = open(filename, O_RDONLY | O_NONBLOCK);
	if (fd == -1) return -1;

	int flags;
	flags = fcntl(fd, F_GETFL, 0) | O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) == -1) return -1;

	f_ctl = fdopen(fd, "r");
	if (f_ctl == NULL) return -1;

	return 0;
}


/*
 * Polls the control file (pipe), non-blockingly, and if a command is received,
 * processes it and updates the RDS data.
 */

int poll_control_pipe() {
	static char buf[CTL_BUFFER_SIZE];

	char *res = fgets(buf, CTL_BUFFER_SIZE, f_ctl);
	if (res == NULL) return -1;
	if (strlen(res) > 3 && res[2] == ' ') {
		char *arg = res+3;
		if (arg[strlen(arg)-1] == '\n') arg[strlen(arg)-1] = 0;
		if (res[0] == 'P' && res[1] == 'I') {
			arg[4] = 0;
			uint16_t pi = strtoul(arg, NULL, 16);
			set_rds_pi(pi);
#ifdef CONTROL_PIPE_MESSAGES
			fprintf(stderr, "PI set to: \"%04X\"\n", pi);
#endif
			return 1;
		}
		if (res[0] == 'P' && res[1] == 'S') {
			arg[8] = 0;
			set_rds_ps(arg);
#ifdef CONTROL_PIPE_MESSAGES
			fprintf(stderr, "PS set to: \"%s\"\n", arg);
#endif
			return 1;
		}
		if (res[0] == 'R' && res[1] == 'T') {
			arg[64] = 0;
			set_rds_rt(arg);
#ifdef CONTROL_PIPE_MESSAGES
			fprintf(stderr, "RT set to: \"%s\"\n", arg);
#endif
			return 1;
		}
		if (res[0] == 'T' && res[1] == 'A') {
			uint8_t ta = (arg[0] == 'O' && arg[1] == 'N');
			set_rds_ta(ta);
#ifdef CONTROL_PIPE_MESSAGES
			fprintf(stderr, "Set TA to %s\n", ta ? "ON" : "OFF");
#endif
			return 1;
		}
		if (res[0] == 'T' && res[1] == 'P') {
			uint8_t tp = (arg[0] == 'O' && arg[1] == 'N');
			set_rds_tp(tp);
#ifdef CONTROL_PIPE_MESSAGES
			fprintf(stderr, "Set TP to %s\n", tp ? "ON" : "OFF");
#endif
			return 1;
		}
		if (res[0] == 'M' && res[1] == 'S') {
			uint8_t ms = (arg[0] == 'O' && arg[1] == 'N');
			set_rds_ms(ms);
#ifdef CONTROL_PIPE_MESSAGES
			fprintf(stderr, "Set MS to %s\n", ms ? "ON" : "OFF");
#endif
			return 1;
		}
		if (res[0] == 'A' && res[1] == 'B') {
			uint8_t ab = (arg[0] == 'A');
			set_rds_ab(ab);
#ifdef CONTROL_PIPE_MESSAGES
			fprintf(stderr, "Set AB to %s\n", ab ? "A" : "B");
#endif
			return 1;
		}
		if (res[0] == 'D' && res[1] == 'I') {
			uint8_t di = strtoul(arg, NULL, 10);
			set_rds_di(di);
#ifdef CONTROL_PIPE_MESSAGES
			fprintf(stderr, "DI value set to %u\n", di);
#endif
			return 1;
		}
	}

	if (strlen(res) > 4 && res[3] == ' ') {
		char *arg = res+4;
		if (arg[strlen(arg)-1] == '\n') arg[strlen(arg)-1] = 0;
		if (res[0] == 'P' && res[1] == 'T' && res[2] == 'Y') {
			uint8_t pty = strtoul(arg, NULL, 10);
			if (pty <= 31) {
				set_rds_pty(pty);
#ifdef CONTROL_PIPE_MESSAGES
				if (!pty) {
					fprintf(stderr, "PTY disabled\n");
				} else {
					fprintf(stderr, "PTY set to: %u\n", pty);
				}
			} else {
				fprintf(stderr, "Wrong PTY identifier! The PTY range is 0 - 31.\n");
#endif
			}
			return 1;
		}
		if (res[0] == 'R' && res[1] == 'T' && res[2] == 'P') {
			uint8_t tags[8];
			if (sscanf(arg, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu", &tags[0], &tags[1], &tags[2], &tags[3], &tags[4], &tags[5]) == 6) {
#ifdef CONTROL_PIPE_MESSAGES
				fprintf(stderr, "RT+ tag 1: type: %u, start: %u, length: %u\n", tags[0], tags[1], tags[2]);
				fprintf(stderr, "RT+ tag 2: type: %u, start: %u, length: %u\n", tags[3], tags[4], tags[5]);
#endif
				set_rds_rtplus_tags((uint8_t *)tags);
			}
#ifdef CONTROL_PIPE_MESSAGES
			else {
				fprintf(stderr, "Could not parse RT+ tag info.\n");
			}
#endif
			return 1;
		}
		if (res[0] == 'M' && res[1] == 'P' && res[2] == 'X') {
			uint8_t gains[5];
			if (sscanf(arg, "%hhu,%hhu,%hhu,%hhu,%hhu", &gains[0], &gains[1], &gains[2], &gains[3], &gains[4]) == 5) {
				for (int i = 0; i < 5; i++) {
					set_carrier_volume(i, gains[i]);
				}
			}
			return 1;
		}
		if (res[0] == 'V' && res[1] == 'O' && res[2] == 'L') {
			set_output_volume(strtoul(arg, NULL, 10));
			return 1;
		}
	}
	if (strlen(res) > 5 && res[4] == ' ') {
		char *arg = res+5;
		if (arg[strlen(arg)-1] == '\n') arg[strlen(arg)-1] = 0;
		if (res[0] == 'R' && res[1] == 'T' && res[2] == 'P' && res[3] == 'F') {
			uint8_t running, toggle;
			if (sscanf(arg, "%hhu,%hhu", &running, &toggle) == 2) {
#ifdef CONTROL_PIPE_MESSAGES
				fprintf(stderr, "RT+ flags: running: %u, toggle: %u\n", running, toggle);
#endif
				set_rds_rtplus_flags(running, toggle);
			}
#ifdef CONTROL_PIPE_MESSAGES
			else {
				fprintf(stderr, "Could not parse RT+ flags.\n");
			}
#endif
			return 1;
		}
		if (res[0] == 'P' && res[1] == 'T' && res[2] == 'Y' && res[3] == 'N') {
			arg[8] = 0;
			if (arg[0] == 'O' && arg[1] == 'F' && arg[2] == 'F') {
#ifdef CONTROL_PIPE_MESSAGES
				fprintf(stderr, "PTYN disabled\n");
#endif
				char tmp[8] = {0};
				set_rds_ptyn(tmp);
			} else {
#ifdef CONTROL_PIPE_MESSAGES
				fprintf(stderr, "PTYN set to: \"%s\"\n", arg);
#endif
				set_rds_ptyn(arg);
			}
			return 1;
		}
	}
	return -1;
}

int close_control_pipe() {
	if (f_ctl) {
		return fclose(f_ctl);
	} else {
		return 0;
	}
}
