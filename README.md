# mpxgen
Based on PiFmAdv (https://github.com/miegl/PiFmAdv) which is based on PiFmRds (https://github.com/ChristopheJacquet/PiFmRds)

This program generates FM multiplex baseband audio that can be fed through a 192 kHz capable sound card to a mono FM transmitter. This includes stereo audio as well as realtime RDS data. This is a work in progress.

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
