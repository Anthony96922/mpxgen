CC = gcc
CFLAGS = -Wall -std=gnu99 -O2

MPXGEN_OBJ = mpx_gen.o rds.o waveforms.o fm_mpx.o control_pipe.o

app: $(MPXGEN_OBJ)
	$(CC) -o mpxgen $(MPXGEN_OBJ) -lm -lsndfile -lsamplerate -lao

clean:
	rm -f *.o
