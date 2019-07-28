CC = gcc
STD_CFLAGS = -Wall -c -O2

CFLAGS = $(STD_CFLAGS)

app: mpx_gen.o rds.o waveforms.o fm_mpx.o control_pipe.o
	$(CC) -o mpxgen mpx_gen.o rds.o waveforms.o fm_mpx.o control_pipe.o -lm -lsndfile -lsamplerate -lao

rds.o: rds.c
	$(CC) $(CFLAGS) rds.c

control_pipe.o: control_pipe.c
	$(CC) $(CFLAGS) control_pipe.c

waveforms.o: waveforms.c
	$(CC) $(CFLAGS) waveforms.c

fm_mpx.o: fm_mpx.c
	$(CC) $(CFLAGS) fm_mpx.c

mpx_gen.o: mpx_gen.c
	$(CC) $(CFLAGS) mpx_gen.c

clean:
	rm -f *.o mpxgen
