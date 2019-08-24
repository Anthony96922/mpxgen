CC = gcc
CFLAGS = -Wall -std=gnu99 -O2

MPXGEN_OBJ = mpx_gen.o rds.o waveforms.o fm_mpx.o control_pipe.o

mpxgen: $(MPXGEN_OBJ)
	$(CC) $(MPXGEN_OBJ) -o mpxgen -lm -lsndfile -lsamplerate -lao

control_pipe.o: control_pipe.c control_pipe.h rds.h

fm_mpx.o: rds.h fm_mpx.h

mpx_gen.o: rds.h fm_mpx.h control_pipe.h

clean:
	rm -f *.o
