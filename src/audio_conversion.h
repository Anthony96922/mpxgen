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

extern void float2char(float *inbuf, char *outbuf, size_t inbufsize);
extern void char2float(char *inbuf, float *outbuf, size_t inbufsize);
extern void short2float(short *inbuf, float *outbuf, size_t inbufsize);
extern void stereoize(char *inbuf, char *outbuf, size_t inbufsize);
extern void stereoizef(float *inbuf, float *outbuf, size_t inbufsize);
