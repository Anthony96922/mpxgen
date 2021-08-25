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

// MPX carrier index
enum mpx_carrier_index {
	CARRIER_19K,
	CARRIER_38K,
	CARRIER_57K,
#ifdef RDS2
	CARRIER_67K,
	CARRIER_71K,
	CARRIER_76K,
#endif
};

extern void create_mpx_carriers(uint32_t sample_rate);
extern float get_carrier(uint8_t num);
extern float get_cos_carrier(uint8_t num);
extern void update_carrier_phase();
extern void clear_mpx_carriers();
