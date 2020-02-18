# mpxgen
Based on [PiFmAdv](https://github.com/miegl/PiFmAdv) which is based on [PiFmRds](https://github.com/ChristopheJacquet/PiFmRds)

This program generates FM multiplex baseband audio that can be fed through a 192 kHz capable sound card to a mono FM transmitter. This includes stereo audio as well as realtime RDS data.

#### Features
- Low resource requirements
- Built-in low-pass filtering
- Support for basic RDS data fields: PS, RT, PTY and AF
- RDS can be updated through control pipe
- RT+ support

It won't replace commercial broadcasting software but it uses less resources and doesn't require a GUI to use.

#### To do
- Improve input resampling
- JACK support (?)

## Build
This app depends on the sndfile, ao and samplerate libraries. On Ubuntu-like distros, use `sudo apt-get install libsndfile1-dev libao-dev libsamplerate0-dev` to install them.

Once those are installed, run
```sh
git clone https://github.com/Anthony96922/mpxgen
cd mpxgen/src
make
```

## How to use
Simply run:
```
./mpxgen
```
This will produce only an RDS subcarrier with no audio. If you have an FM transmitter plugged in to the sound card, tune an RDS-enabled radio to your transmitter's frequency. You should see "mpxgen" appear on the display. Your transmitter must be able to pass the entire MPX spectrum for RDS to work.

To test audio output, you can use the provided stereo_44100.wav file.
```
./mpxgen --audio stereo_44100.wav
```
If the audio sounds distorted, you may be overmodulating the transmitter. Adjust the sound card's volume until audio sounds clear and no distortion can be heard.

There are more options that can be given to mpxgen:
```
--audio
  Audio file to play as audio. The sample rate does not matter: mpxgen will
  resample and filter it. If a stereo file is provided, mpxgen
  will produce an FM Stereo signal. Example: --audio sound.wav .
  The supported formats depend on libsndfile. This includes WAV and
  Ogg/Vorbis (among others) but not MP3. Specify '-' as the file name to
  read audio data on standard input (useful for piping audio into mpxgen, see below).

--pi
  PI code of the RDS broadcast. 4 hexadecimal digits. Example: --pi FFFF .

--ps
  Station name (Program Service name, PS) of the RDS broadcast.
  Limit: 8 characters. Example: --ps KPSK-FM .

--rt
  Radiotext (RT). Limit: 64 characters. Example: --rt 'Hello, world!' .

--af
  List of alternative frequencies (AF). Multiple AFs can be used.
  Example:  --af 107.9 --af 99.2 .

--pty
  Program Type. Valid range: 0 - 31. Example: --pty 9 (US: Top 40).
  See https://en.wikipedia.org/wiki/Radio_Data_System for more program types.

--tp
  Traffic program.  Default: 0.

--ptyn
  Program Type Name is a more specific format. Example: --ptyn CHR.

--mpx
  MPX output volume in percent. Default 100. Use this if your sound card does not have a software volume control.

--ctl
  Named pipe (FIFO) to use as a control channel to change PS and RT and others at run-time (see below).

--rds
  RDS broadcast switch. Enabled by default.

--wait
  Wait for the the audio pipe or terminate as soon as there is no audio. It's set to 1 by default.

--output-file
  outputs raw PCM data to a file instead of playing through the sound card. FIFO pipes can be specified.
```

### Piping audio into mpxgen
If you use the argument `--audio -`, mpxgen reads audio data on standard input. This allows you to pipe the output of a program into mpxgen. For instance, this can be used to read MP3 files using Sox:
```
sox -t mp3 <file or stream URL> -t wav - | ./mpxgen --audio -
```
Basic audio processing with ffmpeg:
```
ffmpeg -i <file or stream URL> \
  -af "
    deesser,
    volume=23dB,
    acrossover[a][b],
    [a]acompressor=threshold=0.001:release=5000:makeup=8[aa],
    [b]acompressor=threshold=0.001:release=5000:makeup=8[ba],
    [aa][ba]amix,
    bass=gain=2,
    volume=-6dB,
    aemphasis=mode=production:type=75fm,
    alimiter
  " \
  -f wav - | ./mpxgen --audio -
```
Or to pipe the AUX input of a sound card into mpxgen:
```
arecord -fS16_LE -r 44100 -Dplughw:1,0 -c 2 -  | ./mpxgen --audio -
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
cat >rds_ctl
PS MyText
RT A text to be sent as radiotext
PTY 10
TA ON
PS OtherTxt
TA OFF
...
```
Every line must start with either `PS`, `RT`, `TA` or `PTY`, followed by one space character, and the desired value. Any other line format is silently ignored. `TA ON` switches the Traffic Announcement flag to *on*, and any other value switches it to *off*.

### RT+
Mpxgen implements RT+ to allow some radios to display indivdual MP3-like metadata tags like artist and song titles from RT.

Syntax for RT+ is comma-separated values specifying content type, start offset and length. RT+ flags use a similar syntax.
```
RTP <content type 1>,<start 1>,<length 1>,<content type 2>,<start 2>,<length 2>
RTPF <running bit>,<toggle bit>
```
For more information, see [RDSPlus_Description.pdf](https://web.archive.org/web/20170312183239/http://www.nprlabs.org/sites/nprlabs/files/documents/pad/RDSPlus_Description.pdf)
