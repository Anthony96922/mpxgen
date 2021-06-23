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

/* 2-channel FIR filter struct */
typedef struct filter_t {
	uint32_t sample_rate;
	uint16_t index;
	uint16_t size;
	uint16_t half_size;
	float *in[2];

	// coefficients of the low-pass FIR filter
	float *filter;

	float out[2];
} filter_t;

// filter state
static filter_t *fir_low_pass;

static inline void fir_filter_init(filter_t *flt, uint32_t sample_rate, uint16_t half_size) {

	flt = malloc(sizeof(filter_t));

	flt->sample_rate = sample_rate;
	flt->half_size = half_size;
	flt->size = 2 * half_size - 1;

	// setup input buffers
	flt->in[0] = malloc(half_size * sizeof(float));
	flt->in[1] = malloc(half_size * sizeof(float));

	flt->filter = malloc(half_size * sizeof(float));

	// Here we divide this coefficient by two because it will be counted twice
	// when applying the filter
	flt->filter[half_size-1] = 2 * 24000 / sample_rate / 2;

	// Only store half of the filter since it is symmetric
	for(int i=1; i<half_size; i++) {
		float tmp;
		tmp = sin(2 * M_PI * 24000 * i / sample_rate) / (M_PI * i); // sinc
		tmp *= .54 - .46 * cos(2 * M_PI * (i+half_size) / (2*half_size)); // Hamming window
		flt->filter[half_size-1-i] = tmp;
	}
}

static inline void fir_filter_add(filter_t *flt, float *in_buffer) {
	flt->in[0][flt->index] = in_buffer[0];
	flt->in[1][flt->index] = in_buffer[1];
	if (++flt->index > flt->size) flt->index = 0;
}

static inline void fir_filter_apply(filter_t *flt) {

	/* As the FIR filter is symmetric, we do not multiply all
	   the coefficients independently, but two-by-two, thus reducing
	   the total number of multiplications by a factor of two
	 */
	int16_t ifbi = flt->index;  // ifbi = increasing FIR Buffer Index
	int16_t dfbi = flt->index;  // dfbi = decreasing FIR Buffer Index
	flt->out[0] = 0;
	flt->out[1] = 0;
	for(int fi=0; fi < flt->size; fi++) {  // fi = Filter Index
		if(--dfbi == -1) dfbi = flt->size-1;
		flt->out[0] += flt->filter[fi] * (flt->in[0][ifbi] + flt->in[0][dfbi]);
		flt->out[1] += flt->filter[fi] * (flt->in[1][ifbi] + flt->in[1][dfbi]);
		if(++ifbi == flt->size) ifbi = 0;
	}
	// End of FIR filter
}

static inline void fir_filter_get(filter_t *flt, float *out) {
	out[0] = flt->out[0];
	out[1] = flt->out[1];
}

static inline void fir_filter_exit(filter_t *flt) {
	free(flt->in[0]);
	free(flt->in[1]);
	free(flt->filter);
	free(flt);
}

void fm_mpx_init() {
	create_mpx_carriers();
	init_hilbert_transformer();
	fir_filter_init(fir_low_pass, MPX_SAMPLE_RATE, 30);
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

	float lowpass_filter_in[2];
	float lowpass_filter_out[2];
	float out_left, out_right;
	float out_mono, out_stereo;

	for (int i = 0; i < NUM_AUDIO_FRAMES_OUT; i++) {
		lowpass_filter_in[0] = in_audio[j+0];
		lowpass_filter_in[1] = in_audio[j+1];

		// First store the current sample(s) into the FIR filter's ring buffer
		fir_filter_add(fir_low_pass, lowpass_filter_in);

		// Now apply the FIR low-pass filter
		fir_filter_apply(fir_low_pass);

		fir_filter_get(fir_low_pass, lowpass_filter_out);

		// L/R signals
		out_left  = lowpass_filter_out[0];
		out_right = lowpass_filter_out[1];

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
	fir_filter_exit(fir_low_pass);
}
