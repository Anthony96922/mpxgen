## Mpxgen command list

This is a complete list of commands Mpxgen recognizes. The API is not yet stable.

Create the control pipe and enable FIFO control:
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
Every line must start with a valid command, followed by one space character, and the desired value. Any other line format is silently ignored. `TA ON` switches the Traffic Announcement flag to *on*, and any other value switches it to *off*.

### Commands

#### `PI`
Sets the PI code. This takes 4 hexadecimal digits.

`PI 1000`

#### `PS`
The Program Service text. Maximum is 8 characters. This is usually static, such as the station's callsign, but can be dynamically updated.

`PS Hello`

#### `RT`
The Radiotext to be displayed. This can be up to 64 characters.

`RT This is a Radiotext message`

#### `TA`
To signal to receivers that there is traffic information currently being broadcast.

`TA 1`

#### `TP`
To signal to receivers that the broadcast can carry traffic info.

`TP 1`

#### `MS`
The Music/Speech flag. Music is 1 and speech is 0.

`MS 1`

#### `AB`
The Radiotext A/B flag. This should not be used, as updates to RT automatically toggle this flag.

`AB A`

#### `DI`
Decoder Identification. A 4-bit decimal number. Usually only the "stereo" flag (1) is set.

`DI 1`

#### `ST`
Pick between normal (0) and "polar" (1) stereo. Don't use polar stereo unless you know what it is and have a need for it.

`ST 0`

#### `PTY`
Set the Program Type. Used to identify the format the station is broadcasting. Valid range is 0-31. Each code corresponds to a Program Type text.

`PTY 0`

#### `MPX`
Set volumes in percent modulation for individual MPX subcarrier signals.

`MPX 9,9,9,9,9`

#### `VOL`
Set the output volume in percent.

`VOL 100`

#### `PPM`
Sets the output sampling rate offset in PPM. This can be used to compensate for clock drift in the sound card.

`PPM -20`

#### `PTYN`
Program Type Name. Used for broadcasting a more specific format identifier. `PTYN OFF` disables broadcasting the PTYN.

`PTYN CHR`

### RadioText Plus
Mpxgen implements RT+ to allow some radios to display indivdual MP3-like metadata tags like artist and song titles from within RT.

For more information, see [EBU Technical Review: RadioText Plus](https://tech.ebu.ch/docs/techreview/trev_307-radiotext.pdf)

#### `RTP`
Radiotext Plus tagging data. Comma-separated values specifying content type, start offset and length. Format: `<content type 1>,<start 1>,<length 1>,<content type 2>,<start 2>,<length 2>`.

`RTP 0,0,0,0,0,0`

#### `RTPF`
Sets the Radiotext Plus "Running" and "Toggle" flags.

`RTPF 1,0`
