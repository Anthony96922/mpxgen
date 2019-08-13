CC = gcc
CFLAGS = -Wall -std=gnu99 -O2

MPXGEN_OBJ = mpx_gen.o rds.o waveforms.o fm_mpx.o control_pipe.o

mpxgen: $(MPXGEN_OBJ)
	$(CC) $(MPXGEN_OBJ) -o mpxgen -lm -lsndfile -lsamplerate -lao

clean:
	rm -f *.o
