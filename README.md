# mpxgen
This program generates FM multiplex baseband audio that can be output to a mono FM transmitter. This includes stereo audio as well as realtime RDS data.

##### This is a work in progress! The audio input handling is not complete and buggy. This app works best as a standalone RDS encoder for now.

#### Features
- Low resource requirements
- Built-in low-pass filtering
- Support for basic RDS data fields: PS, RT, PTY and AF
- RDS items can be updated through control pipe
- RT+ support

#### To do
- Threading
- Input and output buffering
- Basic limiting (?)
- RDS2 capabilities

Mpxgen's RDS encoder in action: https://www.youtube.com/watch?v=ORAMpYhDcVY

## Build
This app depends on the sndfile, ao and samplerate libraries. On Ubuntu-like distros, use `sudo apt-get install libsndfile1-dev libao-dev libsamplerate0-dev` to install them.

Once those are installed, run
```sh
git clone https://github.com/Anthony96922/mpxgen
cd mpxgen/src
make
```

To update, just run `git pull` in the directory and the latest changes will be downloaded. Don't forget to run `make` afterwards.

## How to use
Before running, make sure you're in the audio group to access the sound card.

### Standalone RDS encoder
Simply run:
```
./mpxgen
```
This will produce only an RDS subcarrier with no audio. If you have an FM transmitter plugged in to the sound card, tune an RDS-enabled radio to your transmitter's frequency. You should see "Mpxgen" appear on the display. Your transmitter must be able to pass the entire MPX spectrum for RDS to work.

### With audio
To test audio output, you can use the provided stereo_44100.wav file.
```
./mpxgen --audio stereo_44100.wav
```
If the audio sounds distorted, you may be overmodulating the transmitter. Adjust the output volume (see `--volume` option) until audio sounds clear and no distortion can be heard.

You may use MPXtool to calibrate the output volume so the pilot tone is exactly 9%.

There are more options that can be given to mpxgen:
```
-a / --audio        Audio file to play as audio. If a stereo file is provided, mpxgen
                    will enable its stereo encoder and produce an FM Stereo signal.
                    Example: --audio sound.wav .

                    The supported formats depend on libsndfile. This includes WAV and
                    Ogg/Vorbis (among others) but not MP3. Specify '-' as the file name
                    to read audio data on standard input (useful for piping audio into
                    mpxgen, see below).

-o / --output-file  Outputs WAVE data to a file instead of playing through the sound card.
                    FIFO pipes can be specified. When "-" is used, raw PCM audio data without
                    WAVE headers is output.

-m / --mpx          MPX output volume in percent. Default is 50.

-x / --ppm          Sound card clock correction. This configures the output resampler
                    to compensate for any possible clock drift on the output sound
                    card. Usually not needed.

-W / --wait         Wait for the the audio pipe or terminate as soon as there is no audio.
                    Works for file or pipe input only. Enabled by default.

-R / --rds          RDS broadcast switch. Enabled by default.

-i / --pi           PI code of the RDS broadcast. 4 hexadecimal digits. Example: --pi FFFF .

-s / --ps           Station name (Program Service name) of the RDS broadcast.
                    Limit: 8 characters. Example: --ps KPSK1007 .

-r / --rt           RadioText (RT). Limit: 64 characters. Example: --rt 'Hello, world!' .

-p / --pty          Program Type. Valid range: 0 - 31. Example: --pty 9 (US: Top 40).
                    Default is 0 (None).
                    See https://en.wikipedia.org/wiki/Radio_Data_System for more program types.

-T / --tp           Traffic Program. Default: 0.

-A / --af           List of alternative frequencies (AF). Multiple AFs can be listed.
                    Example: --af 107.9 --af 99.2 .

-P / --ptyn         Program Type Name. Used to indicate a more specific format.
                    Example: --ptyn CHR.

-S / --callsign     Provide an FM callsign and Mpxgen will use it to calculate the PI code
                    for your station. Not case sensitive.  Works only in the USA.
                    Example: --callsign KPSK .

-C / --ctl          Named pipe (FIFO) to use as a control channel to change PS, RT
                    and others at run-time (see below).
```

### Piping audio into mpxgen
If you use the argument `--audio -`, mpxgen reads audio data on standard input. This allows you to pipe the output of a program into mpxgen.

The input gain is 6dB so audio needs to be reduced by -6dB to avoid clipping.

Basic audio processing with ffmpeg:
```
ffmpeg -i <file name or stream URL> \
  -af "
    acompressor=level_in=3:threshold=0.25:ratio=10:attack=2000:release=9000,
    acrossover=split=200|500|1000|4000[a0][a1][a2][a3][a4],
    [a0]acompressor=level_in=2:threshold=0.1:release=5000:makeup=3[b0];
    [a1]acompressor=level_in=2:threshold=0.1:release=5000:makeup=3[b1];
    [a2]acompressor=level_in=2:threshold=0.1:release=5000:makeup=3[b2];
    [a3]acompressor=level_in=2:threshold=0.1:release=5000:makeup=3[b3];
    [a4]acompressor=level_in=2:threshold=0.1:release=5000:makeup=3[b4];
    [b0][b1][b2][b3][b4]amix=inputs=5,
    acompressor=threshold=0.25:ratio=3:attack=1000:release=1000,
    aemphasis=mode=production:type=75fm,
    acrossover=split=200|500|1000|4000[c0][c1][c2][c3][c4],
    [c0]alimiter=level=disabled:attack=10:release=10[d0];
    [c1]alimiter=level=disabled:attack=10:release=10[d1];
    [c2]alimiter=level=disabled:attack=10:release=10[d2];
    [c3]alimiter=level=disabled:attack=10:release=10[d3];
    [c4]alimiter=level=disabled:attack=10:release=10[d4];
    [d0][d1][d2][d3][d4]amix=inputs=5,
    aresample=96k,
    alimiter=limit=0.5:level=disabled:attack=10:release=10,
    aresample=48k,
    firequalizer=gain='if(lt(f,16000), 0, -inf)'
  " \
  -ac 2 -f wav - | ./mpxgen --audio -
```

### Changing PS, RT, TA and PTY at run-time
You can control PS, RT, TA (Traffic Announcement flag) and PTY (Program Type) at run-time using a named pipe (FIFO). For this run mpxgen with the `--ctl` argument.

Scripts can be written to obtain and send "now playing" text data to Mpxgen for dynamically updated RDS.

See the [command list](command_list.md) for a complete list of valid commands.

### RDS2 (WIP)
Mpxgen has an WIP implementation of RDS2. Support for RDS2 features will be implemented once the spec has been released.

#### Credits
Based on [PiFmAdv](https://github.com/miegl/PiFmAdv) which is based on [PiFmRds](https://github.com/ChristopheJacquet/PiFmRds)
