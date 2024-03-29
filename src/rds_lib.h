/*
 * mpxgen - FM multiplex encoder with Stereo and RDS
 * Copyright (C) 2021 Anthony96922
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

extern char *get_pty(uint8_t region, uint8_t pty);
extern void add_checkwords(uint16_t *blocks, uint8_t *bits);
extern uint16_t callsign2pi(char *callsign);

// TMC
extern uint16_t tmc_encrypt(uint16_t loc, uint16_t key);
extern uint16_t tmc_decrypt(uint16_t loc, uint16_t key);
