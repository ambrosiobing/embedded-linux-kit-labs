/* P18, the host half of the USB-CDC recorder.
 *
 * The lab is one number with a second number beside it: how many samples
 * arrived, and how many did not. Everything here exists to make those two
 * honest, and to keep them apart from a third that is often confused with
 * them, the rate the stream actually ran at.
 *
 * Three rules, the same three the sibling lab is written against.
 *
 * No allocation on the data path. Every buffer is a fixed array sized at
 * compile time. A line longer than the buffer is refused whole and counted,
 * never split into two samples that were never sent.
 *
 * Library code returns a status and never exits. Only main.c decides that the
 * program stops, which is what lets the same code sit behind a service later.
 *
 * Nothing is repaired. A gap is counted and left as a gap. A sink that
 * interpolates over a hole has discarded the only measurement the lab makes.
 *
 * One more, particular to this lab. The loss and the rate are two separate
 * faults with two separate causes, and a stream that runs at half its
 * intended rate while losing nothing is a different problem from a stream at
 * the right rate with holes in it. They are measured separately, reported
 * separately and gated separately, and no single word is ever printed that
 * would mean either.
 */
#ifndef P18_H
#define P18_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../../lib/linereader.h"
#include "../../lib/seqtrack.h"

/* ---------------------------------------------------------------- limits */

/* The line buffer is the shared reader's, so that the size a line is
 * measured against and the size it is read into cannot drift apart. The
 * optional session of the chapter prints five fields, so the margin is
 * large. */
#define P18_LINE_MAX LINEREADER_MAX

/* The faults a synthetic stream can be told to carry. */
#define P18_FAULTS_MAX 32

#define P18_RATE_DEFAULT 1000.0
#define P18_SECONDS_DEFAULT 10.0
#define P18_MAX_LOSS_PCT_DEFAULT 0.1
#define P18_RATE_TOLERANCE_DEFAULT 0.10

/* Below this fraction of the duration the stream should have taken, the
 * source was not running in real time and no rate can be claimed from it. A
 * file on a disk is read in milliseconds; a probe at one kilohertz is not. */
#define P18_REALTIME_FRACTION 0.5

typedef enum {
    P18_OK = 0,
    P18_E_IO,
    P18_E_FORMAT,
    P18_E_RANGE,
    P18_E_ARG,
    P18_E_EOF
} p18_status;

const char *p18_strerror(p18_status s);

/* ---------------------------------------------------------------- reading */

/* A line reader with a fixed buffer and no cleverness.
 *
 * The one thing it does that fgets alone does not is refuse a line that does
 * not fit. fgets would hand back the first part and then the remainder, and
 * the remainder would parse as a sequence number that nobody sent. That is a
 * recorder inventing data, which is worse than a recorder losing it, so an
 * over-long line is counted and discarded to the next newline.
 *
 * The reader itself is in lib/linereader.h, shared with P06, which needs the
 * same refusal for a different reason: a partial frame painted on the glass
 * looks like a real reading and is not. The name stays p18_reader here
 * because the suite and the chapter use it. */
typedef linereader p18_reader;

p18_status p18_reader_open(p18_reader *r, const char *path);
void p18_reader_close(p18_reader *r);
/* P18_OK with *line pointing into the reader's own buffer, valid until the
 * next call. P18_E_EOF at the end of the stream. */
p18_status p18_reader_line(p18_reader *r, const char **line, size_t *len);

/* The leading decimal field of a line, and nothing else. The rest of the line
 * is the payload, it belongs to whoever wrote the firmware, and this program
 * does not interpret it. */
p18_status p18_parse_seq(const char *line, uint32_t *seq);

/* ---------------------------------------------------------------- synthesis */

typedef enum {
    P18_FAULT_DROP = 0,   /* the sample is never sent */
    P18_FAULT_DUP,        /* the sample is sent twice */
    P18_FAULT_SWAP,       /* the sample is sent after the one that follows it */
    P18_FAULT_GARBLE,     /* the line carries no sequence number */
    P18_FAULT_LONG        /* the line is longer than any reader's buffer */
} p18_fault_kind;

typedef struct {
    uint32_t at;          /* the sequence number the fault applies to */
    p18_fault_kind kind;
} p18_fault;

typedef struct {
    uint32_t samples;
    uint32_t start_seq;   /* so that a test can place the wrap where it wants it */
    uint32_t seed;
    p18_fault faults[P18_FAULTS_MAX];
    uint16_t fault_count;
} p18_synth;

void p18_synth_init(p18_synth *s);
p18_status p18_synth_add_fault(p18_synth *s, p18_fault_kind kind, uint32_t at);
p18_status p18_synth_write(const p18_synth *s, const char *path);

/* ---------------------------------------------------------------- the run */

typedef struct {
    seqtrack seq;
    uint32_t samples;       /* lines that carried a sequence number and were kept */
    uint32_t malformed;     /* lines that carried none */
    uint32_t long_lines;
    uint32_t blank_lines;
    double elapsed_s;
    double nominal_rate_hz;
    double observed_rate_hz;
    bool rate_measured;     /* false when the source was not running in real time */
    bool deadline_hit;
    bool truncated;         /* stopped at --max rather than at the end */
} p18_run;

void p18_run_init(p18_run *run, double nominal_rate_hz);

/* Read source, write every sample line to out, and account for the sequence
 * numbers on the way. out may be NULL to count without keeping a capture.
 * seconds <= 0 means no deadline, max_samples == 0 means no limit.
 *
 * The deadline is tested between lines. A stream that stops sending entirely
 * leaves this waiting inside a read, which is what timeout(1) is for, and the
 * chapter's command uses it. */
p18_status p18_run_capture(const char *source, const char *out,
                           double seconds, uint32_t max_samples,
                           p18_run *run);

double p18_loss_percent(const p18_run *run);

/* ---------------------------------------------------------------- report */

p18_status p18_report_write(const p18_run *run, const char *path);
p18_status p18_report_read(p18_run *run, const char *path);

typedef struct {
    double max_loss_pct;
    double rate_tolerance;  /* a fraction, so 0.10 is ten per cent either way */
    bool require_rate;      /* refuse a report whose rate was never measured */
} p18_gate;

void p18_gate_init(p18_gate *g);

typedef struct {
    bool loss_ok;
    bool rate_ok;
    bool integrity_ok;      /* no over-long and no malformed lines */
    double loss_pct;
    double rate_ratio;      /* observed over nominal, 1.0 when in agreement */
} p18_verdict;

/* Fills the verdict. Returns P18_OK whether or not the gate passed: whether
 * the run passed is the verdict's business, and whether the question could be
 * answered at all is this status. */
p18_status p18_gate_apply(const p18_gate *g, const p18_run *run, p18_verdict *v);
bool p18_verdict_passed(const p18_verdict *v);

#endif /* P18_H */
