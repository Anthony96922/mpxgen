CC = gcc
CFLAGS = -Wall -O2 -std=gnu99 -pedantic

MPXGEN_OBJ = mpx_gen.o rds.o waveforms.o fm_mpx.o control_pipe.o cpu.o mpx_carriers.o

mpxgen: $(MPXGEN_OBJ)
	$(CC) $(MPXGEN_OBJ) -o mpxgen -lm -lsndfile -lsamplerate -lao

control_pipe.o: control_pipe.h rds.h
fm_mpx.o: rds.h fm_mpx.h mpx_carriers.h
mpx_gen.o: rds.h fm_mpx.h control_pipe.h cpu.h
rds.o: rds.h fm_mpx.h
cpu.o: cpu.h
mpx_carriers.o: mpx_carriers.h

clean:
	rm -f *.o
