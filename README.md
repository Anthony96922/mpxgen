# mpxgen
Based on PiFmAdv (https://github.com/miegl/PiFmAdv) which is based on PiFmRds (https://github.com/ChristopheJacquet/PiFmRds)

This program generates MPX baseband audio that can be fed through a 192 kHz capable sound card to a mono FM transmitter. This includes stereo audio as well as realtime RDS data. This is a work in progress.

Libraries needed:
- libsndfile

To build:

```
git clone https://github.com/tanaka1892/mpxgen
cd mpxgen/src
make
```

To test:

`./mpxgen --audio stereo_44100.wav`

A new audio file named "test.wav" will be made. This file is at 228 kHz sample rate so it needs to be resampled. ffmpeg can be used to do this. Adjust volume as necessary.


`ffmpeg -i test.wav -ar 192k mpx.wav`


Play the newly created audio file:

`aplay mpx.wav`

This is a work in progress. Some things that need to be done:
- Internally resample the audio to 192 kHz
- Directly output to a sound card (through ALSA / libao)
- Allow adjusting output volume if sound card doesn't have a volume control
- Bring RDS encoder up to par with my PiFmAdv fork
