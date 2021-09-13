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

#include "common.h"

#include "rds.h"
#ifdef RDS2
#include "rds2.h"
#endif
#include "fm_mpx.h"
#include "mpx_carriers.h"
#include "ssb.h"

static float mpx_vol;

void set_output_volume(uint8_t vol) {
	if (vol > 100) vol = 100;
	mpx_vol = (vol / 100.0);
}

// subcarrier volumes
static float volumes[] = {
	0.09, // pilot tone: 9% modulation
	0.09, // RDS: 4.5% modulation

	0.09, // RDS 2
	0.09,
	0.09
};

void set_carrier_volume(uint8_t carrier, int8_t new_volume) {

	if (new_volume == -1) return;
	if (carrier > 4) return;

	if (new_volume >= 0 && new_volume <= 15) {
		volumes[carrier] = new_volume / 100.0;
	} else {
		volumes[carrier] = 0.09;
	}
}

/*
 * 2-channel FIR filter struct
 *
 */
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
static struct filter_t *fir_low_pass;

static void fir_filter_init(struct filter_t *flt, uint32_t sample_rate, uint16_t half_size) {

	flt = malloc(sizeof(struct filter_t));

	flt->sample_rate = sample_rate;
	flt->half_size = half_size;
	flt->size = 2 * half_size - 1;

	// setup input buffers
	flt->in[0] = malloc(flt->size * sizeof(float));
	flt->in[1] = malloc(flt->size * sizeof(float));
	flt->filter = malloc(flt->half_size * sizeof(float));

	// Here we divide this coefficient by two because it will be counted twice
	// when applying the filter
	flt->filter[half_size-1] = 2 * 15500 / sample_rate / 2;

	// Only store half of the filter since it is symmetric
	float filter, window;
	for (int i = 1; i < half_size; i++) {
		filter = sin(2 * M_PI * 24000 * i / sample_rate) / (M_PI * i); // sinc
		window = .54 - .46 * cos(2 * M_PI * (half_size + i) / (2 * half_size)); // Hamming window
		flt->filter[half_size-1-i] = filter * window;
	}
}

static inline void fir_filter_add(struct filter_t *flt, float *in_buffer) {
	flt->in[0][flt->index] = in_buffer[0];
	flt->in[1][flt->index] = in_buffer[1];
	if (++flt->index == flt->size) flt->index = 0;
}

static inline void fir_filter_apply(struct filter_t *flt) {

	/* As the FIR filter is symmetric, we do not multiply all
	   the coefficients independently, but two-by-two, thus reducing
	   the total number of multiplications by a factor of two
	 */
	int16_t ifbi = flt->index;  // ifbi = increasing FIR Buffer Index
	int16_t dfbi = flt->index;  // dfbi = decreasing FIR Buffer Index
	flt->out[0] = 0;
	flt->out[1] = 0;
	for (int fi = 0; fi < flt->half_size; fi++) {  // fi = Filter Index
		if(--dfbi == -1) dfbi = flt->size-1;
		flt->out[0] += flt->filter[fi] * (flt->in[0][ifbi] + flt->in[0][dfbi]);
		flt->out[1] += flt->filter[fi] * (flt->in[1][ifbi] + flt->in[1][dfbi]);
		if(++ifbi == flt->size) ifbi = 0;
	}
	// End of FIR filter
}

static inline void fir_filter_get(struct filter_t *flt, float *out) {
	out[0] = flt->out[0];
	out[1] = flt->out[1];
}

static void fir_filter_exit(struct filter_t *flt) {
	free(flt->in[0]);
	free(flt->in[1]);
	free(flt->filter);
	free(flt);
}

/*
 * Filter delay line
 *
 */
typedef struct delay_line_t {
	float *buffer;
	uint32_t delay;
	uint32_t idx;
} delay_line_t;

/*
 * delay buffers for hilbert transform
 *
 */
static struct delay_line_t left_delay;
static struct delay_line_t right_delay;

/*
 * filter delays needed for SSB
 *
 */
static void init_delay_line(struct delay_line_t *delay_line) {
	delay_line->buffer = malloc(MPX_SAMPLE_RATE * sizeof(float));
}

static void set_delay_line(struct delay_line_t *delay_line, uint32_t new_delay) {
	delay_line->delay = new_delay;
}

static inline float delay_line(struct delay_line_t *delay_line, float in) {
	delay_line->buffer[delay_line->idx++] = in;
	if (delay_line->idx >= delay_line->delay) delay_line->idx = 0;
	return delay_line->buffer[delay_line->idx];
}

static void exit_delay_line(struct delay_line_t *delay_line) {
	free(delay_line->buffer);
}

void fm_mpx_init() {
	create_mpx_carriers(MPX_SAMPLE_RATE);
	init_hilbert_transformer();
	fir_filter_init(fir_low_pass, MPX_SAMPLE_RATE, 128);
	init_delay_line(&left_delay);
	init_delay_line(&right_delay);
	set_delay_line(&left_delay, HT_FILTER_HALF_SIZE);
	set_delay_line(&right_delay, HT_FILTER_HALF_SIZE);
}

/*
 * SSB modulator
 *
 * Creates a single sideband signal
 * 0: LSB
 * 1: USB
 *
 * Might be removed in favor of the asymmetric DSB modulator below
 */
static inline float get_ssb(float in, float in_delayed, float sin, float cos, uint8_t sideband) {
	float ht;
	float inphase, quadrature;

	// perform a 90 degree phase shift of all frequency components
	ht = get_hilbert(in);

	// I/Q components
	inphase    = in_delayed * cos;
	quadrature = ht * sin;

	return	!sideband ?
		inphase + quadrature : // lsb
		inphase - quadrature;  // usb
}

/*
 * Asymmetric DSB configuration
 */
struct {
	float lsb_power;
	float usb_power;
} asym_dsb_config;

/*
 * Asymmetric DSB modulator
 *
 * LSB/USB range: [-1,1]
 * 0 is symmetric
 */
static inline float get_asym_dsb(float in, float in_delayed, float sin, float cos) {
	float ht;
	float inphase, quadrature;

	// perform a 90 degree phase shift of all frequency components
	ht = get_hilbert(in);

	// I/Q components
	inphase    = in_delayed * cos;
	quadrature = ht * sin;

	return	(inphase + quadrature) * asym_dsb_config.lsb_power + // lsb
		(inphase - quadrature) * asym_dsb_config.usb_power;  // usb
}

void set_asym_dsb(float asymmetry) {
	asym_dsb_config.lsb_power = fabsf(1.0 - asymmetry) / 2.0;
	asym_dsb_config.usb_power = fabsf(1.0 + asymmetry) / 2.0;
}

void fm_mpx_get_samples(float *in, float *out) {
	uint16_t j = 0;

	float lowpass_filter_in[2];
	float lowpass_filter_out[2];
	float out_left, out_right;
	float out_mono, out_stereo;
	// delayed versions of the above for SSB filter
	float out_left_delayed, out_right_delayed;
	float out_mono_delayed, out_stereo_delayed;

	for (int i = 0; i < NUM_MPX_FRAMES; i++) {
		lowpass_filter_in[0] = in[j+0];
		lowpass_filter_in[1] = in[j+1];

		// First store the current sample(s) into the FIR filter's ring buffer
		fir_filter_add(fir_low_pass, lowpass_filter_in);

		// Now apply the FIR low-pass filter
		fir_filter_apply(fir_low_pass);

		fir_filter_get(fir_low_pass, lowpass_filter_out);

		// L/R signals
		out_left  = lowpass_filter_out[0];
		out_right = lowpass_filter_out[1];
		out_left_delayed  = delay_line(&left_delay, out_left);
		out_right_delayed = delay_line(&right_delay, out_right);

		// Create sum and difference signals
		out_mono   = out_left + out_right;
		out_stereo = out_left - out_right;
		out_mono_delayed   = out_left_delayed + out_right_delayed;
		out_stereo_delayed = out_left_delayed - out_right_delayed;

		// clear old buffer
		out[j] = 0.0;

		if (1) { // SSB mode
			// delay mono so it is in sync with stereo
			out[j] += out_mono_delayed * 0.45 +
				get_cos_carrier(CARRIER_19K) * volumes[0];

			out[j] +=
				get_ssb(out_stereo,
					out_stereo_delayed,
					get_carrier(CARRIER_38K),
					get_cos_carrier(CARRIER_38K),
					0 /* LSB */) * 0.45;

		} else {
			// audio signals need to be limited to 45% to remain within modulation limits
			out[j] += out_mono * 0.45 +
				get_cos_carrier(CARRIER_19K) * volumes[0] +
				get_cos_carrier(CARRIER_38K) * out_stereo * 0.45;
		}

		out[j] += get_cos_carrier(CARRIER_57K) * get_rds_sample(0) * volumes[1];
#ifdef RDS2
		out[j] += get_cos_carrier(CARRIER_67K) * get_rds_sample(1) * volumes[2];
		out[j] += get_cos_carrier(CARRIER_71K) * get_rds_sample(2) * volumes[3];
		out[j] += get_cos_carrier(CARRIER_76K) * get_rds_sample(3) * volumes[4];
#endif

		update_carrier_phase();

		out[j] *= mpx_vol;
		out[j+1] = out[j];
		j += 2;
	}
}

void fm_rds_get_samples(float *out) {
	uint16_t j = 0;

	for (int i = 0; i < NUM_MPX_FRAMES; i++) {
		out[j] = 0;

		// Pilot tone for calibration
		out[j] += get_cos_carrier(CARRIER_19K) * volumes[0];

		out[j] += get_cos_carrier(CARRIER_57K) * get_rds_sample(0) * volumes[1];
#ifdef RDS2
		out[j] += get_cos_carrier(CARRIER_67K) * get_rds_sample(1) * volumes[2];
		out[j] += get_cos_carrier(CARRIER_71K) * get_rds_sample(2) * volumes[3];
		out[j] += get_cos_carrier(CARRIER_76K) * get_rds_sample(3) * volumes[4];
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
	exit_delay_line(&left_delay);
	exit_delay_line(&right_delay);
}
