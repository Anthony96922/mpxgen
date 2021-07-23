
# change to "1" to enable RDS2 capabilities (don't forget to run "make clean" first)
RDS2 = 0

CC = gcc
CFLAGS = -Wall -O2 -std=gnu99 -pedantic

obj = mpx_gen.o rds.o waveforms.o fm_mpx.o control_pipe.o mpx_carriers.o \
	resampler.o input.o file_input.o ssb.o output.o alsa_output.o \
	file_output.o alsa_input.o
libs = -lm -lsndfile -lsamplerate -lpthread -lasound

ifeq ($(RDS2), 1)
	CFLAGS += -DRDS2
	obj += rds2.o rds2_image_data.o
endif

all: mpxgen

mpxgen: $(obj)
	$(CC) $(obj) $(libs) -o mpxgen

control_pipe.o: control_pipe.h fm_mpx.h rds.h

fm_mpx.o: rds.h fm_mpx.h mpx_carriers.h rds2.h ssb.h

mpx_gen.o: rds.h fm_mpx.h control_pipe.h

file_output.o: file_output.h

input.o: input.h

mpx_carriers.o: fm_mpx.h

clean:
	rm -f *.o
