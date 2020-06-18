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

float carrier_19k[] = {0, 0.5, 0.8660254, 1, 0.8660254, 0.5, 0, -0.5, -0.8660254, -1, -0.8660254, -0.5};
float carrier_38k[] = {0, 0.8660254, 0.8660254, 0, -0.8660254, -0.8660254};
float carrier_57k[] = {0, 1, 0, -1};
#ifdef RDS2
/* RDS 2 carriers
 * 66.5/71.25/76
 */
float carrier_67k[] = {0, 0.9659258, -0.5, -0.7071068, 0.8660254, 0.258819, -1, 0.258819, 0.8660254, -0.7071068, -0.5, 0.9659258, 0, -0.9659258, 0.5, 0.7071068, -0.8660254, -0.258819, 1, -0.258819, -0.8660254, 0.7071068, 0.5, -0.9659258};
float carrier_71k[] = {0, 0.9238795, -0.7071068, -0.3826834, 1, -0.3826834, -0.7071068, 0.9238795, 0, -0.9238795, 0.7071068, 0.3826834, -1, 0.3826834, 0.7071068, -0.9238795};
float carrier_76k[] = {0, 0.8660254, -0.8660254, 0, 0.8660254, -0.8660254};
#endif

int phase_19k;
int phase_38k;
int phase_57k;
#ifdef RDS2
int phase_67k;
int phase_71k;
int phase_76k;
#endif

float level_19k = 1;
float level_38k = 1;
float level_57k = 1;

float get_19k_carrier() {
	return carrier_19k[phase_19k] * level_19k;
}

float get_38k_carrier() {
	return carrier_38k[phase_38k] * level_38k;
}

float get_57k_carrier() {
	return carrier_57k[phase_57k] * level_57k;
}

#ifdef RDS2
float get_67k_carrier() {
	return carrier_67k[phase_67k];
}

float get_71k_carrier() {
	return carrier_71k[phase_71k];
}

float get_76k_carrier() {
	return carrier_76k[phase_76k];
}
#endif

void update_carrier_phase() {
	if (++phase_19k == 12) phase_19k = 0;
	if (++phase_38k == 6) phase_38k = 0;
	if (++phase_57k == 4) phase_57k = 0;
#ifdef RDS2
	if (++phase_67k == 24) phase_67k = 0;
	if (++phase_71k == 16) phase_71k = 0;
	if (++phase_76k == 6) phase_76k = 0;
#endif
}

void set_19k_level(int new_level) {
	if (new_level == -1) return;
	level_19k = (new_level / 100.0);
}

void set_38k_level(int new_level) {
	if (new_level == -1) return;
	level_38k = (new_level / 100.0);
}

void set_57k_level(int new_level) {
	if (new_level == -1) return;
	level_57k = (new_level / 100.0);
}
