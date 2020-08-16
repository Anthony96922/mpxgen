# mpxgen
This program generates FM multiplex baseband audio that can be output to a mono FM transmitter. This includes stereo audio as well as realtime RDS data.

##### This is a work in progress! While I try to add new features myself, feel free to contribute and help improve this app!

#### Features
- Low resource requirements
- Built-in low-pass filtering
- Support for basic RDS data fields: PS, RT, PTY and AF
- RDS items can be updated through control pipe
- RT+ support
- Experimental RDS2 capabilities

#### To do
- Threading (<- very important!)
- Basic limiting (?)

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

Simply run:
```
./mpxgen
```
This will produce only an RDS subcarrier with no audio. If you have an FM transmitter plugged in to the sound card, tune an RDS-enabled radio to your transmitter's frequency. You should see "Mpxgen" appear on the display. Your transmitter must be able to pass the entire MPX spectrum for RDS to work.

To test audio output, you can use the provided stereo_44100.wav file.
```
./mpxgen --audio stereo_44100.wav
```
If the audio sounds distorted, you may be overmodulating the transmitter. Adjust the output volume (see `--volume` option) until audio sounds clear and no distortion can be heard.

You may use MPXtool to calibrate the pilot tone level so the level is exactly 8%.

There are more options that can be given to mpxgen:
```
-a / --audio        Audio file to play as audio. If a stereo file is provided, mpxgen
                    will enable it stereo encoder and produce an FM Stereo signal.
                    Example: --audio sound.wav .

                    The supported formats depend on libsndfile. This includes WAV and
                    Ogg/Vorbis (among others) but not MP3. Specify '-' as the file name to
                    read audio data on standard input (useful for piping audio into mpxgen, see below).

-o / --output-file  Outputs WAVE data to a file instead of playing through the sound card. FIFO pipes
                    can be specified. When "-" is used, raw PCM audio data without WAVE headers is output.

-m / --mpx          MPX output volume in percent. Default is 50.

-x / --ppm          Sound card clock correction. This configures the output resampler to compensate
                    for any possible clock drift on the output sound card. Usually not needed.

-W / --wait         Wait for the the audio pipe or terminate as soon as there is no audio. Enabled by default.

-R / --rds          RDS broadcast switch. Enabled by default.

-i / --pi           PI code of the RDS broadcast. 4 hexadecimal digits. Example: --pi FFFF .

-s / --ps           Station name (Program Service name) of the RDS broadcast.
                    Limit: 8 characters. Example: --ps KPSK1007 .

-r / --rt           RadioText (RT). Limit: 64 characters. Example: --rt 'Hello, world!' .

-p / --pty          Program Type. Valid range: 0 - 31. Example: --pty 9 (US: Top 40).
                    See https://en.wikipedia.org/wiki/Radio_Data_System for more program types.

-T / --tp           Traffic Program. Default: 0.

-A / --af           List of alternative frequencies (AF). Multiple AFs can be listed.
                    Example: --af 107.9 --af 99.2 .

-P / --ptyn         Program Type Name. Used to indicate a more specific format. Example: --ptyn CHR.


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
    acrossover=split=200|500|1000|4000[a0][a1][a2][a3][a4],
    [a0]acompressor=level_in=5:threshold=0.050:release=5000:makeup=5[b0];
    [a1]acompressor=level_in=5:threshold=0.025:release=5000:makeup=5[b1];
    [a2]acompressor=level_in=5:threshold=0.025:release=5000:makeup=5[b2];
    [a3]acompressor=level_in=5:threshold=0.025:release=5000:makeup=5[b3];
    [a4]acompressor=level_in=5:threshold=0.025:release=5000:makeup=5[b4];
    [b0][b1][b2][b3][b4]amix=inputs=5,
    aemphasis=mode=production:type=75fm,
    acrossover=split=200|500|1000|4000[c0][c1][c2][c3][c4],
    [c0]alimiter=level=disabled:attack=0.1:release=1[d0];
    [c1]alimiter=level=disabled:attack=0.1:release=1[d1];
    [c2]alimiter=level=disabled:attack=0.1:release=1[d2];
    [c3]alimiter=level=disabled:attack=0.1:release=1[d3];
    [c4]alimiter=level=disabled:attack=0.1:release=1[d4];
    [d0][d1][d2][d3][d4]amix=inputs=5,
    acompressor=threshold=0.25:ratio=6:attack=1000:release=1000,
    alimiter=limit=0.55:level=disabled:attack=0.1:release=1,
    firequalizer=gain='if(lt(f,16000), 0, -inf)'
  " \
  -f wav - | ./mpxgen --audio -
```

### Changing PS, RT, TA and PTY at run-time
You can control PS, RT, TA (Traffic Announcement flag) and PTY (Program Type) at run-time using a named pipe (FIFO). For this run mpxgen with the `--ctl` argument.

Example:
```
mkfifo rds_ctl
./mpxgen --ctl rds_ctl
```
Then you can send “commands” to change PS, RT, TA and PTY:
```
cat > rds_ctl
PS MyText
RT A text to be sent as radiotext
PTY 10
TA ON
PS OtherTxt
TA OFF
...
```
Every line must start with either `PS`, `RT`, `TA` or `PTY`, followed by one space character, and the desired value. Any other line format is silently ignored. `TA ON` switches the Traffic Announcement flag to *on*, and any other value switches it to *off*.

Scripts can be written to obtain "now playing" data and feed it into Mpxgen for dynamically updated RDS.

### RT+
Mpxgen implements RT+ to allow some radios to display indivdual MP3-like metadata tags like artist and song titles from within RT.

Syntax for RT+ is comma-separated values specifying content type, start offset and length. RT+ flags use a similar syntax.
```
RTP <content type 1>,<start 1>,<length 1>,<content type 2>,<start 2>,<length 2>
RTPF <running bit>,<toggle bit>
```
For more information, see [EBU Technical Review: RadioText Plus](https://tech.ebu.ch/docs/techreview/trev_307-radiotext.pdf)

### RDS2 (experimental)
Mpxgen has an untested implementation of RDS2. To enable, look for the `RDS2 = 0` in the Makefile and change the `0` to a `1`. Then run `make clean && make` to rebuild with RDS2 capabilities.

#### Credits
Based on [PiFmAdv](https://github.com/miegl/PiFmAdv) which is based on [PiFmRds](https://github.com/ChristopheJacquet/PiFmRds)
