# mpxgen
This program generates FM multiplex baseband audio that can be sent to a mono FM transmitter. This includes stereo audio as well as realtime RDS data.

![Mpxgen on Eton](doc/mpxgen.jpg)

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

#### Planned features
- Basic audio processing
- RDS2 capabilities
- SSB stereo
- Configuration file

Mpxgen's RDS encoder in action: https://www.youtube.com/watch?v=ORAMpYhDcVY

## Build
This app depends on libsndfile, libasound and libsamplerate. On Ubuntu-like distros, use `sudo apt-get install libsndfile1-dev libasound2-dev libsamplerate0-dev` to install them.

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
This mode is for those who already have a stereo encoder and want to add RDS to their station.

Simply run:
```
./mpxgen
```
This will produce only an RDS subcarrier with no audio. If you have an FM transmitter plugged in to the sound card, tune an RDS-enabled radio to your transmitter's frequency. You should see "Mpxgen" appear on the display. Your transmitter must be able to pass the entire MPX spectrum for RDS to work. Use `--ctl` to change RDS settings at run-time.

The resulting RDS signal may then be added to the MPX signal using an active mixer. Make sure you're not overdeviating. If using Stereo Tool for audio processing and stereo encoding, enable and use the "Use SCA Input as external RDS input" option instead.

### With audio
To test audio output, you can use the provided stereo_44100.wav file.
```
./mpxgen --audio stereo_44100.wav
```
If the audio sounds distorted, you may be overmodulating the transmitter. Adjust the output volume (see `--mpx` option) until audio sounds clear and no distortion can be heard.

You may use MpxTool to calibrate the output volume so the pilot tone is exactly 9%.

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
                    for your station. Not case sensitive. Works only in the USA.
                    Example: --callsign KPSK .

-C / --ctl          Named pipe (FIFO) to use as a control channel to change PS, RT
                    and others at run-time (see below).
```

### Piping audio into mpxgen
If you use the argument `--audio -`, Mpxgen reads audio data on standard input. This allows you to pipe the output of a program into Mpxgen.

The input gain is 6dB so audio needs to be reduced by -6dB to avoid clipping.

### Changing PS, RT, TA and PTY at run-time
You can control PS, RT, TA (Traffic Announcement flag) and PTY (Program Type) at run-time using a named pipe (FIFO). For this run mpxgen with the `--ctl` argument.

Scripts can be written to obtain and send "now playing" text data to Mpxgen for dynamically updated RDS.

See the [command list](doc/command_list.md) for a complete list of valid commands.

### RDS2 (WIP)
Mpxgen has a WIP implementation of RDS2. Support for RDS2 features will be implemented once the spec has been released.

#### Credits
Based on [PiFmAdv](https://github.com/miegl/PiFmAdv) which is based on [PiFmRds](https://github.com/ChristopheJacquet/PiFmRds)
