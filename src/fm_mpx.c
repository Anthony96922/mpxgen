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

static const float carrier_frequencies[] = {
	19000.0, // pilot tone
	38000.0, // stereo difference
	57000.0, // RDS

#ifdef RDS2
	// RDS 2
	66500.0, // stream 1
	71250.0, // stream 2
	76000.0,  // stream 2
#endif
	0.0 // terminator
};

/*
 * filter state
 *
 */
static struct filter_t fir_low_pass;

/*
 * delay buffers for hilbert transform
 *
 */
static struct delay_line_t left_delay;
static struct delay_line_t right_delay;

/*
 * Local cscillator object
 * this is where the MPX waveforms are stored
 *
 */
static struct osc_t mpx_osc;

/*
 * Hilbert transformer object
 *
 */
static struct hilbert_fir_t ssb_ht;

void set_output_volume(uint8_t vol) {
	if (vol > 100) vol = 100;
	mpx_vol = (vol / 100.0f);
}

// subcarrier volumes
static float volumes[] = {
	0.09, // pilot tone: 9% modulation
	0.09, // RDS: 4.5% modulation

	0.09, // RDS 2
	0.09,
	0.09
};

void set_carrier_volume(uint8_t carrier, uint8_t new_volume) {
	if (carrier > 4) return;
	if (new_volume >= 15) volumes[carrier] = 0.09f;
	volumes[carrier] = new_volume / 100.0f;
}

static void init_fir_filter(struct filter_t *flt, uint32_t sample_rate, uint16_t half_size) {

	memset(flt, 0, sizeof(struct filter_t));

	flt->sample_rate = sample_rate;
	flt->half_size = half_size;
	flt->size = 2 * half_size - 1;

	// setup input buffers
	flt->in[0] = malloc(flt->size * sizeof(float));
	flt->in[1] = malloc(flt->size * sizeof(float));
	flt->filter = malloc(flt->half_size * sizeof(float));

	// Here we divide this coefficient by two because it will be counted twice
	// when applying the filter
	flt->filter[half_size-1] = (float)(2 * 24000 / sample_rate / 2);

	// Only store half of the filter since it is symmetric
	double filter, window;
	for (int i = 1; i < half_size; i++) {
		filter = sin(M_2PI * 24000 * i / sample_rate) / (M_PI * i); // sinc
		window = 0.54 - 0.46 * cos(M_2PI * (double)(half_size + i) / (double)(2 * half_size)); // Hamming window
		flt->filter[half_size-1-i] = (float)(filter * window);
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

static void exit_fir_filter(struct filter_t *flt) {
	free(flt->in[0]);
	free(flt->in[1]);
	free(flt->filter);
}

/*
 * filter delays needed for SSB
 *
 */
static void init_delay_line(struct delay_line_t *delay_line, uint32_t sample_rate) {
	delay_line->buffer = malloc(sample_rate * sizeof(float));
	memset(delay_line->buffer, 0, sample_rate * sizeof(float));
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
	init_osc(&mpx_osc, MPX_SAMPLE_RATE, carrier_frequencies);
	init_hilbert_transformer(&ssb_ht, 512);
	init_fir_filter(&fir_low_pass, MPX_SAMPLE_RATE, 128);
	init_delay_line(&left_delay, MPX_SAMPLE_RATE);
	init_delay_line(&right_delay, MPX_SAMPLE_RATE);
	set_delay_line(&left_delay, 256 /* half of HT filter size */);
	set_delay_line(&right_delay, 256 /* half of HT filter size */);
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
	ht = get_hilbert(&ssb_ht, in);

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
	ht = get_hilbert(&ssb_ht, in);

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

	for (int i = 0; i < NUM_MPX_FRAMES_IN; i++) {
		lowpass_filter_in[0] = in[j+0];
		lowpass_filter_in[1] = in[j+1];

		// First store the current sample(s) into the FIR filter's ring buffer
		fir_filter_add(&fir_low_pass, lowpass_filter_in);

		// Now apply the FIR low-pass filter
		fir_filter_apply(&fir_low_pass);

		fir_filter_get(&fir_low_pass, lowpass_filter_out);

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
		out[j] = 0.0f;

		if (1) { // SSB mode
			// delay mono so it is in sync with stereo
			out[j] += out_mono_delayed * 0.45 +
				get_wave(&mpx_osc, CARRIER_19K, 1) * volumes[0];

			out[j] +=
				get_ssb(out_stereo,
					out_stereo_delayed,
					get_wave(&mpx_osc, CARRIER_38K, 0),
					get_wave(&mpx_osc, CARRIER_38K, 1),
					0 /* LSB */) * 0.45;

		} else {
			// audio signals need to be limited to 45% to remain within modulation limits
			out[j] += out_mono * 0.45 +
				get_wave(&mpx_osc, CARRIER_19K, 1) * volumes[0] +
				get_wave(&mpx_osc, CARRIER_38K, 1) * out_stereo * 0.45;
		}

		out[j] += get_wave(&mpx_osc, CARRIER_57K, 1) * get_rds_sample(0) * volumes[1];
#ifdef RDS2
		out[j] += get_wave(&mpx_osc, CARRIER_67K, 1) * get_rds_sample(1) * volumes[2];
		out[j] += get_wave(&mpx_osc, CARRIER_71K, 1) * get_rds_sample(2) * volumes[3];
		out[j] += get_wave(&mpx_osc, CARRIER_76K, 1) * get_rds_sample(3) * volumes[4];
#endif

		update_osc_phase(&mpx_osc);

		out[j] *= mpx_vol;
		out[j+1] = out[j];
		j += 2;
	}
}

void fm_rds_get_samples(float *out) {
	uint16_t j = 0;

	for (int i = 0; i < NUM_MPX_FRAMES_IN; i++) {
		out[j] = 0.0f;

		// Pilot tone for calibration
		out[j] += get_wave(&mpx_osc, CARRIER_19K, 1) * volumes[0];

		//out[j] += get_wave(&mpx_osc, CARRIER_57K, 1) * get_rds_sample(0) * volumes[1];
#ifdef RDS2
		out[j] += get_wave(&mpx_osc, CARRIER_67K, 1) * get_rds_sample(1) * volumes[2];
		out[j] += get_wave(&mpx_osc, CARRIER_71K, 1) * get_rds_sample(2) * volumes[3];
		out[j] += get_wave(&mpx_osc, CARRIER_76K, 1) * get_rds_sample(3) * volumes[4];
#endif

		update_osc_phase(&mpx_osc);

		out[j] *= mpx_vol;
		out[j+1] = out[j];
		j += 2;
	}
}

void fm_mpx_exit() {
	exit_hilbert_transformer(&ssb_ht);
	exit_osc(&mpx_osc);
	exit_fir_filter(&fir_low_pass);
	exit_delay_line(&left_delay);
	exit_delay_line(&right_delay);
}
