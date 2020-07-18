
# change to "1" to enable RDS2 capabilities (don't forget to run "make clean" first)
RDS2 = 0

# enable experimental ALSA input
ALSA = 0

CC = gcc
CFLAGS = -Wall -O2 -std=gnu99 -pedantic

obj = mpx_gen.o rds.o waveforms.o fm_mpx.o control_pipe.o mpx_carriers.o resampler.o input.o audio_conversion.o
libs = -lm -lsndfile -lsamplerate -lao

ifeq ($(RDS2), 1)
	CFLAGS += -DRDS2
	obj += rds2.o rds2_image_data.o
endif

ifeq ($(ALSA), 1)
	CFLAGS += -DALSA
	obj += alsa_input.o
	libs += -lasound
endif

mpxgen: $(obj)
	$(CC) $(obj) $(libs) -o mpxgen

control_pipe.o: control_pipe.h rds.h

fm_mpx.o: rds.h fm_mpx.h mpx_carriers.h

mpx_gen.o: rds.h fm_mpx.h control_pipe.h

rds.o: rds.h fm_mpx.h

mpx_carriers.o: mpx_carriers.h

rds2.o: rds2.h

resampler.o: resampler.h

audio_conversion.o: audio_conversion.h

alsa_input.o: alsa_input.h

clean:
	rm -f *.o
