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

#include <stdlib.h>
#include <math.h>

#include "rds.h"
#ifdef RDS2
#include "rds2.h"
#endif
#include "fm_mpx.h"
#include "mpx_carriers.h"
#include "ssb.h"

#define FIR_HALF_SIZE	30
#define FIR_SIZE	(2*FIR_HALF_SIZE-1)

// coefficients of the low-pass FIR filter
static float *low_pass_fir;
static float *fir_buffer[2];

float mpx_vol;

void set_output_volume(unsigned int vol) {
	if (vol > 100) vol = 100;
	mpx_vol = (vol / 100.0);
}

// subcarrier volumes
static float volumes[] = {
	0.09, // pilot tone: 9% modulation
	0.09, // RDS: 4.5% modulation
	0.09, 0.09, 0.09 // RDS 2
};

void set_carrier_volume(unsigned int carrier, int new_volume) {
	if (new_volume == -1) return;
	if (carrier <= 4) {
		if (new_volume >= 0 && new_volume <= 15) {
			volumes[carrier] = new_volume / 100.0;
		} else {
			volumes[carrier] = 0.09;
		}
	}
}

void fm_mpx_init() {
	create_mpx_carriers();
	init_hilbert_transformer();

	low_pass_fir = malloc(FIR_HALF_SIZE * sizeof(float));
	fir_buffer[0] = malloc(FIR_SIZE * sizeof(float));
	fir_buffer[1] = malloc(FIR_SIZE * sizeof(float));

	// Here we divide this coefficient by two because it will be counted twice
	// when applying the filter
	low_pass_fir[FIR_HALF_SIZE-1] = 2 * 24000 / MPX_SAMPLE_RATE / 2;

	// Only store half of the filter since it is symmetric
	for(int i=1; i<FIR_HALF_SIZE; i++) {
		low_pass_fir[FIR_HALF_SIZE-1-i] =
			sin(2 * M_PI * 24000 * i / MPX_SAMPLE_RATE) / (M_PI * i) // sinc
			* (.54 - .46 * cos(2 * M_PI * (i+FIR_HALF_SIZE) / (2*FIR_HALF_SIZE))); // Hamming window
	}
}

/*
 * SSB modulator
 *
 * Creates a single sideband signal
 * 0: LSB
 * 1: USB
 */
static inline float get_single_sideband(float in, int carrier, int sideband) {
	float ht, delayed;
	float inphase, quadrature;

	ht      = get_hilbert(in);
	delayed = get_hilbert_delay(in);

	// I/Q components
	inphase    = ht * get_cos_carrier(carrier);
	quadrature = delayed * get_carrier(carrier);

	return	!sideband ?
		inphase + quadrature : /* lsb */
		inphase - quadrature;  /* usb */
}

/*
 * Asymmetric DSB modulator
 *
 * LSB/USB range: [-1,1]
 * 0 is symmetric
 */
static inline float get_asymmetric_dsb(float in, int carrier, float asymmetry) {
	float ht, delayed;
	float inphase, quadrature;
	float lsb_power, usb_power;

	ht      = get_hilbert(in);
	delayed = get_hilbert_delay(in);

	// I/Q components
	inphase    = ht * get_cos_carrier(carrier);
	quadrature = delayed * get_carrier(carrier);

	lsb_power = fabsf(1 - asymmetry) / 2;
	usb_power = fabsf(1 + asymmetry) / 2;

	return	(inphase + quadrature) * lsb_power + /* lsb */
		(inphase - quadrature) * usb_power;  /* usb */
}

void fm_mpx_get_samples(float *in_audio, float *out) {
	int j = 0;
	static int fir_index;

	int ifbi, dfbi;
	float out_left, out_right;
	float out_mono, out_stereo;

	for (int i = 0; i < NUM_AUDIO_FRAMES_OUT; i++) {
		// First store the current sample(s) into the FIR filter's ring buffer
		fir_buffer[0][fir_index] = in_audio[j+0];
		fir_buffer[1][fir_index] = in_audio[j+1];
		fir_index++;
		if(fir_index == FIR_SIZE) fir_index = 0;

		// L/R signals
		out_left  = 0;
		out_right = 0;

		// Now apply the FIR low-pass filter

		/* As the FIR filter is symmetric, we do not multiply all
		   the coefficients independently, but two-by-two, thus reducing
		   the total number of multiplications by a factor of two
		 */
		ifbi = fir_index;  // ifbi = increasing FIR Buffer Index
		dfbi = fir_index;  // dfbi = decreasing FIR Buffer Index
		for(int fi=0; fi<FIR_HALF_SIZE; fi++) {  // fi = Filter Index
			dfbi--;
			if(dfbi < 0) dfbi = FIR_SIZE-1;
			out_left  += low_pass_fir[fi] * (fir_buffer[0][ifbi] + fir_buffer[0][dfbi]);
			out_right += low_pass_fir[fi] * (fir_buffer[1][ifbi] + fir_buffer[1][dfbi]);
			ifbi++;
			if(ifbi == FIR_SIZE) ifbi = 0;
		}
		// End of FIR filter

		// Create sum and difference signals
		out_mono   = out_left + out_right;
		out_stereo = out_left - out_right;

		// audio signals need to be limited to 45% to remain within modulation limits
		out[j] = out_mono * 0.45;

		out[j] += get_cos_carrier(CARRIER_57K) * get_rds_sample() * volumes[1];
#ifdef RDS2
		out[j] += get_cos_carrier(CARRIER_67K) * get_rds2_sample(1) * volumes[2];
		out[j] += get_cos_carrier(CARRIER_71K) * get_rds2_sample(2) * volumes[3];
		out[j] += get_cos_carrier(CARRIER_76K) * get_rds2_sample(3) * volumes[4];
#endif

		out[j] +=
			get_cos_carrier(CARRIER_19K) * volumes[0] +
			get_cos_carrier(CARRIER_38K) * out_stereo * 0.45;

		update_carrier_phase();

		out[j] *= mpx_vol;
		out[j+1] = out[j];
		j += 2;
	}
}

void fm_rds_get_samples(float *out) {
	int j = 0;

	for (int i = 0; i < NUM_RDS_FRAMES_IN; i++) {
		// Pilot tone for calibration
		out[j] = get_cos_carrier(CARRIER_19K) * volumes[0];

		out[j] += get_cos_carrier(CARRIER_57K) * get_rds_sample() * volumes[1];
#ifdef RDS2
		out[j] += get_cos_carrier(CARRIER_67K) * get_rds2_sample(1) * volumes[2];
		out[j] += get_cos_carrier(CARRIER_71K) * get_rds2_sample(2) * volumes[3];
		out[j] += get_cos_carrier(CARRIER_76K) * get_rds2_sample(3) * volumes[4];
#endif

		update_carrier_phase();

		out[j] *= mpx_vol;
		out[j+1] = out[j];
		j += 2;
	}
}

void fm_mpx_exit() {
	exit_hilbert_transformer();
	clear_mpx_carriers();
	free(low_pass_fir);
	free(fir_buffer[0]);
	free(fir_buffer[1]);
}
