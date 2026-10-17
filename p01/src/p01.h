/* P01, the shock event publisher, which P09 also runs.
 *
 * Two chapters use this one program. P01 runs it on a Pi 4 with the
 * accelerometer on bus 1 and a Cat-4 modem carrying the publishes. P09 runs
 * it on a NanoPi NEO Air with the same sensor on bus 0, headless, writing to
 * soldered flash. The chapters say the difference is the bus number, so the
 * difference here is an argument rather than a second program.
 *
 * The loop has one piece of state, a cool-off timestamp, and it is a token
 * bucket in its simplest form: at most one event every two seconds, however
 * long the table keeps ringing. Without it one fist produces a burst of
 * twenty publishes and the compression this lab exists for is lost.
 *
 * On the NanoPi that same timestamp is a wear control. Every event that is
 * also appended to the log is a write to flash soldered to the board, and
 * the flash cannot be replaced when it tires. So the budget is arithmetic
 * worth doing before the run rather than after: what reaches the flash is
 * bounded by the cool-off and the fsync interval, and by nothing else.
 *
 * The rules of the volume hold: nothing allocates on the data path, library
 * code returns a status and never exits, and nothing is repaired. A sample
 * that cannot be read is counted, never interpolated.
 *
 * One rule of its own. The decision is taken on the magnitude of the vector
 * and its distance from one gravity, never on a single axis. A sensor lying
 * on its side reads one gravity on a different axis, and a threshold on one
 * axis would call that an event every time the board is picked up.
 */
#ifndef P01_H
#define P01_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../../lib/adxl345.h"
#include "../../lib/linereader.h"

/* The part is in lib/adxl345.h, with P19, which reads the same six bytes
 * and turns them into angles instead of events. These names stay because
 * this lab's chapter and suite use them. */
#define P01_ADDRESS ADXL345_ADDRESS
#define P01_REG_POWER_CTL ADXL345_REG_POWER_CTL
#define P01_REG_DATA_FORMAT ADXL345_REG_DATA_FORMAT
#define P01_REG_DATAX0 ADXL345_REG_DATAX0
#define P01_POWER_MEASURE ADXL345_POWER_MEASURE
#define P01_FORMAT_FULL_RES_16G ADXL345_FORMAT_FULL_RES_16G
#define P01_COUNTS_PER_G ADXL345_COUNTS_PER_G

#define P01_DEADBAND_G 0.35
#define P01_COOLOFF_S 2.0
#define P01_SAMPLE_HZ 20.0
#define P01_FSYNC_S 30.0
#define P01_RECORD_BYTES 48

typedef enum {
    P01_OK = 0,
    P01_E_IO,
    P01_E_FORMAT,
    P01_E_ARG
} p01_status;

const char *p01_strerror(p01_status s);

/* ---------------------------------------------------------------- samples */

typedef adxl345_sample p01_sample;

/* Six bytes as the device returns them, X low first. Two's complement, and
 * the sign extension is the half of this that is usually wrong: a reading of
 * minus one gravity is 0xFF00, not 65280 counts of acceleration. */
void p01_decode(const uint8_t raw[6], p01_sample *out);

/* "f0 ff 10 00 00 01", six byte values in hexadecimal, separated by spaces
 * or commas, and nothing else on the line. Hexadecimal and not "whatever
 * parses", because that is what the device returns, what a bench capture
 * records and what this program writes: one base, so that the two halves
 * cannot disagree about their own file. A leading 0x is accepted. */
bool p01_parse_bytes(const char *text, uint8_t raw[6]);

/* ---------------------------------------------------------------- policy */

typedef struct {
    double deadband_g;
    double cooloff_s;
    double next_allowed;    /* in the caller's timebase */
    bool armed;             /* false until the first sample is seen */

    uint32_t samples;
    uint32_t over_threshold; /* samples that would have published */
    uint32_t events;         /* samples that did */
    uint32_t suppressed;     /* the difference, which is the compression */
    double last_event_at;
    double first_sample_at;
    double last_sample_at;
} p01_policy;

void p01_policy_init(p01_policy *p, double deadband_g, double cooloff_s);
/* True when this sample becomes an event. The clock is an argument, so the
 * suite drives a ten minute run without waiting for one. */
bool p01_policy_offer(p01_policy *p, double now, const p01_sample *s);

/* ---------------------------------------------------------------- the budget */

/* What reaches the radio and the flash in an hour, for a surface that never
 * stops ringing. The worst case is the useful one: a quiet bench produces
 * nothing and proves nothing. */
typedef struct {
    double sample_hz;
    double cooloff_s;
    double fsync_s;
    uint32_t record_bytes;
    double hours;

    double samples;
    double events_without_cooloff;
    double events;
    double bytes_to_flash;
    double bytes_without_cooloff;
    double fsyncs;
    double fsyncs_per_event;      /* the figure that says a timer is used */
    double compression;           /* events without the cool-off, over events */
} p01_budget;

p01_status p01_budget_make(p01_budget *b, double sample_hz, double cooloff_s,
                           double fsync_s, uint32_t record_bytes, double hours);
void p01_budget_print(const p01_budget *b, FILE *out);

/* ---------------------------------------------------------------- replay */

/* A file of samples, one per line:
 *
 *     <seconds> <six bytes>
 *     0.000 00 00 00 00 00 01
 *
 * which is what a bench capture of the raw device looks like with a clock
 * beside it, and what the suite feeds the policy. */
typedef struct {
    p01_policy policy;
    uint32_t lines;
    uint32_t malformed;
    double peak_magnitude;
} p01_run;

p01_status p01_replay(const char *path, double deadband_g, double cooloff_s,
                      p01_run *out);
void p01_run_print(const p01_run *r, FILE *out);

/* ---------------------------------------------------------------- scenes */

typedef enum {
    P01_SCENE_AT_REST = 0,   /* one gravity, nothing happening */
    P01_SCENE_ONE_KNOCK,     /* a single strike and its ringing */
    P01_SCENE_RINGING,       /* a surface that never settles */
    P01_SCENE_ON_ITS_SIDE,   /* one gravity on a different axis, which is not an event */
    P01_SCENE_FREE_FALL      /* zero gravity, which is as far from 1 g as 2 g is */
} p01_scene;

p01_status p01_scene_write(p01_scene scene, double seconds, double sample_hz,
                           const char *path);

/* ---------------------------------------------------------------- the device */

/* The live read, on Linux, through i2c-dev. Returns P01_E_IO when the bus
 * or the device is not there, which on a first run is the normal answer and
 * is why the chapters run i2cdetect first. */
p01_status p01_device_open(int *fd, int bus, uint8_t address);
p01_status p01_device_configure(int fd);
p01_status p01_device_read(int fd, uint8_t raw[6]);
void p01_device_close(int fd);

#endif /* P01_H */
