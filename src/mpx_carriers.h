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
#define CARRIER_19K 0
#define CARRIER_38K 1
#define CARRIER_57K 2
#define CARRIER_67K 3
#define CARRIER_71K 4
#define CARRIER_76K 5
#define CARRIER_31K 6

extern void create_mpx_carriers(int sample_rate);
extern float get_carrier(int num);
extern void update_carrier_phase();
extern void clear_mpx_carriers();
