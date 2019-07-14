# mpxgen
Based on PiFmAdv (https://github.com/miegl/PiFmAdv) which is based on PiFmRds (https://github.com/ChristopheJacquet/PiFmRds)

This program generates MPX baseband audio that can be fed through a 192 kHz capable sound card to a mono FM transmitter. This includes stereo audio as well as realtime RDS data. This is a work in progress.

Libraries needed:
- libsndfile
- libsamplerate

To build:

```
git clone https://github.com/Anthony96922/mpxgen
cd mpxgen/src
make
```

To test:

`./mpxgen --audio stereo_44100.wav`

mpxgen will output to your default sound card.

This is a work in progress. Some things that need to be done:
- Internally resample the audio to 192 kHz *done*
- Directly output to a sound card (through ALSA / libao) *done*
- Allow adjusting output volume if sound card doesn't have a volume control
- Improve RDS encoder feature-set with additions from my PiFmAdv fork
