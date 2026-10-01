/* P04: the host half of the vibration and ultrasound gateway.
 *
 * One header for the whole program. It is small enough that splitting the
 * declarations across six files would cost a reader more than it saves.
 *
 * Three rules shape the code below, and they are the reason it is C.
 *
 *   1. No allocation on the data path. Every buffer is a fixed array sized at
 *      compile time by P04_BLOCK_MAX. A capture whose blocks are longer than
 *      that is refused at the file header, not part way through.
 *   2. Library code returns a status and never calls exit(). Only main.c
 *      decides that the program stops.
 *   3. Samples are int16_t because that is what the probe sends. They become
 *      float only inside the transform, and the conversion is in one place.
 */

#ifndef P04_H
#define P04_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ---------------------------------------------------------------- limits */

/* The transform is radix-2, so a block length must be a power of two. 2048 at
 * the probe's output rate is about 77 ms of signal, which is long enough to
 * resolve the band and short enough that a classification still tracks a
 * touch. */
#define P04_BLOCK_MAX 4096
#define P04_BLOCK_DEFAULT 2048

/* The IIS3DWB runs at a nominal 26.667 kHz. Nothing here assumes it: the rate
 * travels in the capture file and the band edges are computed from it. This is
 * the default a synthetic capture is written with. */
#define P04_RATE_DEFAULT 26667.0f

/* Defaults for the band the chapter cares about, in hertz. The part is flat to
 * roughly 6 kHz, so a band above that measures the anti-alias filter rather
 * than the machine. */
#define P04_BAND_LOW_DEFAULT 2000.0f
#define P04_BAND_HIGH_DEFAULT 6000.0f

/* ---------------------------------------------------------------- status */

typedef enum {
    P04_OK = 0,
    P04_E_IO,        /* the file would not open, read or write */
    P04_E_FORMAT,    /* the bytes are not a capture this program wrote */
    P04_E_RANGE,     /* a value is outside what the fixed buffers allow */
    P04_E_ARG,       /* the caller passed something impossible */
    P04_E_EOF        /* no more blocks, which is not a fault */
} p04_status;

const char *p04_strerror(p04_status s);

/* ---------------------------------------------------------------- blocks */

/* One block of the stream: a sequence number and two channels of the same
 * length. Wideband vibration and the digital microphone are kept apart all the
 * way through, because the lab's whole claim is that they move independently.
 */
typedef struct {
    uint32_t seq;
    uint16_t len;
    int16_t vib[P04_BLOCK_MAX];
    int16_t mic[P04_BLOCK_MAX];
} p04_block;

/* A capture file. The header carries what the analysis needs to know and
 * cannot infer: how long a block is, how fast the samples came, and what one
 * count is worth in g. */
typedef struct {
    FILE *fp;
    bool writing;
    uint16_t block_len;
    uint32_t block_count;   /* as declared in the header */
    uint32_t blocks_done;   /* how many have actually been read or written */
    float rate_hz;
    float g_per_count;
} p04_capture;

p04_status p04_capture_create(p04_capture *c, const char *path, uint16_t block_len,
                              float rate_hz, float g_per_count);
p04_status p04_capture_open(p04_capture *c, const char *path);
p04_status p04_capture_write(p04_capture *c, const p04_block *b);
p04_status p04_capture_read(p04_capture *c, p04_block *b);
p04_status p04_capture_close(p04_capture *c);

/* What the stream did on the way, as opposed to what the signal did. A
 * spectrum over a stream with holes in it is a picture of the holes, so this
 * is reported beside every result rather than folded into it. */
typedef struct {
    uint32_t received;
    uint32_t expected;   /* highest sequence seen, minus the first, plus one */
    uint32_t gaps;       /* how many sequence numbers never arrived */
    uint32_t duplicates;
    uint32_t reorders;
    uint32_t first_seq;
    uint32_t last_seq;
    bool started;
} p04_transport;

void p04_transport_init(p04_transport *t);
void p04_transport_see(p04_transport *t, uint32_t seq);
bool p04_transport_whole(const p04_transport *t);

/* ---------------------------------------------------------------- dsp */

/* A window, computed once and reused. Recomputing a cosine per block is the
 * kind of waste that does not show up on a laptop and does on a Pi 3. */
typedef struct {
    uint16_t len;
    float w[P04_BLOCK_MAX];
    float coherent_gain;   /* sum(w)/len, which the energy is divided by */
} p04_window;

void p04_window_hann(p04_window *win, uint16_t len);

/* In-place iterative radix-2 transform. len must be a power of two and at most
 * P04_BLOCK_MAX. Iterative rather than recursive so the stack cost is known. */
p04_status p04_fft(float *re, float *im, uint16_t len);

/* Magnitude spectrum of a real signal: out holds len/2 + 1 bins. */
p04_status p04_spectrum(const int16_t *x, uint16_t len, const p04_window *win,
                        float *out, uint16_t *out_bins);

/* Which bins a band in hertz covers, given the rate the samples came at. Both
 * ends are clamped to the spectrum, and a band entirely above Nyquist is an
 * error rather than an empty sum that reads as silence. */
p04_status p04_band_bins(uint16_t n_bins, float rate_hz, float low_hz, float high_hz,
                         uint16_t *first, uint16_t *last);

/* Summed power in the band, normalised by the window's coherent gain so that
 * two captures windowed the same way are comparable. */
p04_status p04_band_energy(const int16_t *x, uint16_t len, const p04_window *win,
                           float rate_hz, float low_hz, float high_hz, float *out);

float p04_rms(const int16_t *x, uint16_t len);

/* ---------------------------------------------------------------- analysis */

typedef enum { P04_NORMAL = 0, P04_WARNING = 1, P04_FAULT = 2 } p04_state;

const char *p04_state_name(p04_state s);

/* A baseline and two multipliers of it. The thresholds are ratios rather than
 * absolute energies so that a baseline taken on one mounting does not pretend
 * to apply to another. */
typedef struct {
    float baseline;      /* band energy of a machine at rest */
    float warn_ratio;
    float fault_ratio;
    float band_low_hz;
    float band_high_hz;
    float rate_hz;       /* the rate the baseline was taken at */
    float g_per_count;
    char mounting[128];  /* free text, because the mounting decides the number */
} p04_thresholds;

void p04_thresholds_default(p04_thresholds *t);
p04_state p04_classify(float energy, const p04_thresholds *t, float *ratio_out);

/* The DC level of a block in g. At rest this should be one gravity, and it is
 * the only absolute check available without a reference shaker. */
float p04_dc_level_g(const int16_t *x, uint16_t len, float g_per_count);

/* Was the rate the band edges were computed from the rate the data came at?
 * Different question from whether the stream arrived whole, and it fails for
 * different reasons, so it is reported separately. */
typedef struct {
    float declared_hz;
    float measured_hz;
    float error_fraction;
    bool trustworthy;
} p04_timing;

void p04_timing_check(p04_timing *t, float declared_hz, float measured_hz, float tolerance);

/* ---------------------------------------------------------------- pipeline */

typedef struct {
    uint32_t seq;
    p04_state state;
    float ratio;
    float vib_energy;
    float mic_rms;
} p04_reading;

#define P04_READINGS_MAX 4096

typedef struct {
    p04_reading readings[P04_READINGS_MAX];
    uint32_t count;
    p04_transport transport;
    bool overflowed;     /* more blocks than readings were kept */
} p04_result;

void p04_result_init(p04_result *r);
p04_status p04_run(p04_capture *cap, const p04_thresholds *t, p04_result *out);
p04_state p04_result_worst(const p04_result *r);
float p04_result_mean_ratio(const p04_result *r);
float p04_result_mean_mic_rms(const p04_result *r);

/* ---------------------------------------------------------------- lab book */

/* The baseline is written as a flat key=value file rather than JSON, so that
 * the program depends on nothing and the file can be read and edited with the
 * tools that are on a board. */
p04_status p04_labbook_write(const char *path, const p04_thresholds *t);
p04_status p04_labbook_read(const char *path, p04_thresholds *t);

/* ---------------------------------------------------------------- synth */

/* Synthetic stimuli with the right shape, which is not a model of the probe.
 * A threshold tuned against these is a threshold tuned against these. */
typedef enum { P04_SYNTH_IDLE, P04_SYNTH_TOUCH, P04_SYNTH_SPEECH } p04_synth_kind;

p04_status p04_synth_kind_parse(const char *name, p04_synth_kind *out);
void p04_synth_block(p04_block *b, p04_synth_kind kind, uint32_t seq,
                     uint16_t len, float rate_hz, uint32_t *rng);

#endif /* P04_H */
