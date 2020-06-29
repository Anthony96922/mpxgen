
# change to "1" to enable RDS2 capabilities
RDS2 = 0

CC = gcc
CFLAGS = -Wall -O2 -std=gnu99 -pedantic

MPXGEN_OBJ = mpx_gen.o rds.o waveforms.o fm_mpx.o control_pipe.o mpx_carriers.o resampler.o input.o

ifeq ($(RDS2), 1)
	CFLAGS += -DRDS2
	MPXGEN_OBJ += rds2.o rds2_image_data.o
endif

mpxgen: $(MPXGEN_OBJ)
	$(CC) $(MPXGEN_OBJ) -o mpxgen -lm -lsndfile -lsamplerate -lao

control_pipe.o: control_pipe.h rds.h

fm_mpx.o: rds.h fm_mpx.h mpx_carriers.h

mpx_gen.o: rds.h fm_mpx.h control_pipe.h

rds.o: rds.h fm_mpx.h

mpx_carriers.o: mpx_carriers.h

rds2.o: rds2.h

resampler.o: resampler.h

clean:
	rm -f *.o
