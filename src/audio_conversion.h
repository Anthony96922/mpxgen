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

#include "common.h" // for lround

// float to short
static inline void float2short(float *inbuf, int16_t *outbuf, size_t inbufsize) {
	for (int i = 0; i < inbufsize; i++) {
		outbuf[i] = lround(inbuf[i] * 32767);
	}
}

// converts 16 bit shorts (stored as two 8 bit ints) to floats
static inline void char2float(int8_t *inbuf, float *outbuf, size_t inbufsize) {
	int i = 0, j = 0;

	for (i = 0; i < inbufsize; i++) {
		outbuf[i] = ((inbuf[j+0] & 0xff) | (inbuf[j+1] << 8)) / 32767.0;
		j += 2;
	}
}

// converts 16 bit shorts to floats
static inline void short2float(int16_t *inbuf, float *outbuf, size_t inbufsize) {
	for (int i = 0; i < inbufsize; i++) {
		outbuf[i] = inbuf[i] / 32767.0;
	}
}

// stereoizers
// puts the same stuff into both channels

// s16le
static inline void stereoizes16(int16_t *inbuf, int16_t *outbuf, size_t inbufsize) {
	int i = 0, j = 0;

	for (i = 0; i < inbufsize; i++) {
		outbuf[j+0] = outbuf[j+1] = inbuf[i];
		j += 2;
	}
}

// float
static inline void stereoizef(float *inbuf, float *outbuf, size_t inbufsize) {
	int i = 0, j = 0;

	for (i = 0; i < inbufsize; i++) {
		outbuf[j+0] = outbuf[j+1] = inbuf[i];
		j += 2;
	}
}
