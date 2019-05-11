CC = gcc
STD_CFLAGS = -Wall -Wno-multichar -std=gnu99 -ffast-math -c -O1

CFLAGS = $(STD_CFLAGS) -march=armv7-a -mtune=cortex-a5 -mfloat-abi=hard -mfpu=vfp

app: mpx_gen.o rds.o waveforms.o fm_mpx.o control_pipe.o
	$(CC) -o mpxgen mpx_gen.o rds.o waveforms.o fm_mpx.o control_pipe.o -lm -lsndfile

rds.o: rds.c waveforms.h
	$(CC) $(CFLAGS) rds.c

control_pipe.o: control_pipe.c control_pipe.h rds.h
	$(CC) $(CFLAGS) control_pipe.c

waveforms.o: waveforms.c waveforms.h
	$(CC) $(CFLAGS) waveforms.c

mailbox.o: mailbox.c mailbox.h
	$(CC) $(CFLAGS) mailbox.c

fm_mpx.o: fm_mpx.c fm_mpx.h
	$(CC) $(CFLAGS) fm_mpx.c

mpx_gen.o: mpx_gen.c
	$(CC) $(CFLAGS) mpx_gen.c
clean:
	rm -f *.o mpxgen
