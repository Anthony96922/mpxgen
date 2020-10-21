## Mpxgen command list

This is a complete list of commands Mpxgen recognizes. As features are added, changed or removed, the syntax for some commands may change.

Every line must start with a valid command, followed by one space character, and the desired value. Any other line format is silently ignored.

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

#### `RTP`
Radiotext Plus tagging data. Format: `<content type 1>,<start 1>,<length 1>,<content type 2>,<start 2>,<length 2>`.

`RTP 0,0,0,0,0,0`

#### `MPX`
Set volumes in percent modulation for individual MPX subcarrier signals.

`MPX 9,9,9,9,9`

#### `VOL`
Set the output volume in percent.

`VOL 100`

#### `PPM`
Sets the output sampling rate offset in PPM. This can be used to compensate for clock drift in the sound card.

`PPM -20`

#### `RTPF`
Sets the Radiotext Plus "Running" and "Toggle" flags.

`RTPF 1,0`

#### `PTYN`
Program Type Name. Used for broadcasting a more specific format identifier. `PTYN OFF` disables broadcasting the PTYN.

`PTYN CHR`
