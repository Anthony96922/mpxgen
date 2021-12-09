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

#include <samplerate.h>

#define CONVERTER_TYPE SRC_SINC_FASTEST

extern int8_t resampler_init(SRC_STATE **src_state, uint8_t channels);
extern int8_t resample(SRC_STATE *src_state, SRC_DATA src_data, size_t *frames_generated);
extern void resampler_exit(SRC_STATE *src_state);
