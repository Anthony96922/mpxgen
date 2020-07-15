/*
 * mpxgen - FM multiplex encoder with Stereo and RDS
 * Copyright (C) 2019 Anthony96922
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <unistd.h>
#include "audio_conversion.h"

// converts float into 16 bit shorts (stored as two 8 bit values)
void float2char(float *inbuf, char *outbuf, size_t inbufsize) {
	int j = 0;
	short sample;

	for (int i = 0; i < inbufsize; i++) {
		sample = inbuf[i] * 32767;
		outbuf[j+0] = sample & 255;
		outbuf[j+1] = sample >> 8;
		j += 2;
	}
}

// converts 16 bit shorts (stored as two 8 bit values) into floats
void char2float(char *inbuf, float *outbuf, size_t inbufsize) {
	int j = 0;
	float sample;

	for (int i = 0; i < inbufsize; i++) {
		sample = ((inbuf[j+0] & 255) | ((inbuf[j+1] & 255) << 8)) / 32767.0;
		outbuf[i] = sample;
		j += 2;
	}
}

void stereoize(char *inbuf, char *outbuf, size_t inbufsize) {
	int j = 0;
	int k = 0;

	for (int i = 0; i < inbufsize; i++) {
		outbuf[k+0] = outbuf[k+2] = inbuf[j+0];
		outbuf[k+1] = outbuf[k+3] = inbuf[j+1];
		j += 2;
		k += 4;
	}
}
