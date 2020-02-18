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

float level_19k = 1;
float level_38k = 1;
float level_57k = 1;

float get_19k_carrier() {
        static int phase;
        float c = carrier_19k[phase++] * level_19k;
        if (phase == 12) phase = 0;
        return c;
}

float get_38k_carrier() {
        static int phase;
        float c = carrier_38k[phase++] * level_38k;
        if (phase == 6) phase = 0;
        return c;
}

float get_57k_carrier() {
        static int phase;
        float c = carrier_57k[phase++] * level_57k;
        if (phase == 4) phase = 0;
        return c;
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
