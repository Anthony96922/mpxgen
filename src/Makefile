
# change to "1" to enable RDS2 capabilities (don't forget to run "make clean" first)
RDS2 = 0

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu99 -pedantic

obj = mpx_gen.o rds.o waveforms.o fm_mpx.o control_pipe.o mpx_carriers.o \
	resampler.o input.o file_input.o ssb.o output.o alsa_output.o \
	file_output.o alsa_input.o rds_modulator.o rds_lib.o
libs = -lm -lsndfile -lsamplerate -lpthread -lasound

ifeq ($(RDS2), 1)
	CFLAGS += -DRDS2
	obj += rds2.o rds2_image_data.o
endif

all: mpxgen

mpxgen: $(obj)
	$(CC) $(obj) $(libs) -o mpxgen -s

clean:
	rm -f *.o
