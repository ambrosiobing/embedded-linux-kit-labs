/* P04, the command line.
 *
 * Six subcommands, and only one of them needs hardware:
 *
 *   synth      write a synthetic capture, so the rest can run with no probe
 *   baseline   measure a machine at rest and write the lab book
 *   analyse    classify a capture against a lab book
 *   replay     the same, block by block, for reading rather than for a verdict
 *   transport  report only whether the stream arrived whole
 *   validate   check the capture against gravity, which is the one absolute
 *
 * Exit status is the program's answer, not decoration. Zero means the question
 * was answered. Non-zero means it could not be, and the reason is on stderr.
 * A classification of FAULT is still a zero exit: the program worked, the
 * machine did not, and conflating those two is how a monitoring tool trains
 * its operator to ignore it.
 */

#include "p04.h"

#include <stdlib.h>
#include <string.h>

/* The rate the lab book was written at and the rate a capture arrived at must
 * agree, or the band edges point at the wrong frequencies. One per cent is
 * tighter than any real clock error and looser than a wrong rate. */
#define RATE_TOLERANCE 0.01f

/* A probe at rest reads one gravity. Anything outside this band means the
 * scale, the axis or the mounting is not what the lab book says. */
#define GRAVITY_LIMIT_G 0.25f

#define MAX_DROPS 64

static void usage(FILE *out)
{
    fprintf(out,
        "usage: p04 <command> [options]\n"
        "\n"
        "  synth     --kind idle|touch|speech --blocks N --out FILE\n"
        "            [--len N] [--rate HZ] [--drop A,B,...] [--seed N]\n"
        "  baseline  --source FILE --out FILE [--warn R] [--fault R]\n"
        "            [--band-low HZ] [--band-high HZ] [--mounting TEXT]\n"
        "  analyse   --source FILE --book FILE\n"
        "  replay    --source FILE --book FILE\n"
        "  transport --source FILE\n"
        "  validate  --source FILE --book FILE\n"
        "\n"
        "Every command but a live capture runs with no hardware attached.\n");
}

/* ---------------------------------------------------------------- options */

typedef struct {
    const char *kind;
    const char *source;
    const char *book;
    const char *out;
    const char *mounting;
    long blocks;
    long len;
    double rate;
    double warn;
    double fault;
    double band_low;
    double band_high;
    unsigned long seed;
    uint32_t drops[MAX_DROPS];
    size_t n_drops;
} opts;

static void opts_init(opts *o)
{
    memset(o, 0, sizeof *o);
    o->blocks = 10;
    o->len = P04_BLOCK_DEFAULT;
    o->rate = (double)P04_RATE_DEFAULT;
    o->warn = 1.5;
    o->fault = 3.0;
    o->band_low = (double)P04_BAND_LOW_DEFAULT;
    o->band_high = (double)P04_BAND_HIGH_DEFAULT;
    o->seed = 1u;
    o->mounting = "unrecorded";
}

static bool parse_drops(opts *o, const char *s)
{
    o->n_drops = 0;
    while (*s != '\0') {
        char *end = NULL;
        long v = strtol(s, &end, 10);
        if (end == s || v < 0)
            return false;
        if (o->n_drops >= MAX_DROPS)
            return false;
        o->drops[o->n_drops++] = (uint32_t)v;
        s = end;
        if (*s == ',')
            s++;
        else if (*s != '\0')
            return false;
    }
    return true;
}

static bool dropped(const opts *o, uint32_t seq)
{
    for (size_t i = 0; i < o->n_drops; i++)
        if (o->drops[i] == seq)
            return true;
    return false;
}

/* Returns false and prints the reason when an option is unknown or its value
 * is missing. An unknown option is refused rather than ignored: a typed flag
 * that silently does nothing is how a run gets believed. */
static bool parse_args(opts *o, int argc, char **argv)
{
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        const char *v = (i + 1 < argc) ? argv[i + 1] : NULL;

        #define NEED_VALUE() do { \
            if (v == NULL) { fprintf(stderr, "p04: %s needs a value\n", a); return false; } \
            i++; \
        } while (0)

        if (strcmp(a, "--kind") == 0)            { NEED_VALUE(); o->kind = v; }
        else if (strcmp(a, "--source") == 0)     { NEED_VALUE(); o->source = v; }
        else if (strcmp(a, "--book") == 0)       { NEED_VALUE(); o->book = v; }
        else if (strcmp(a, "--out") == 0)        { NEED_VALUE(); o->out = v; }
        else if (strcmp(a, "--mounting") == 0)   { NEED_VALUE(); o->mounting = v; }
        else if (strcmp(a, "--blocks") == 0)     { NEED_VALUE(); o->blocks = strtol(v, NULL, 10); }
        else if (strcmp(a, "--len") == 0)        { NEED_VALUE(); o->len = strtol(v, NULL, 10); }
        else if (strcmp(a, "--rate") == 0)       { NEED_VALUE(); o->rate = atof(v); }
        else if (strcmp(a, "--true-rate") == 0)  { NEED_VALUE(); o->rate = atof(v); }
        else if (strcmp(a, "--warn") == 0)       { NEED_VALUE(); o->warn = atof(v); }
        else if (strcmp(a, "--fault") == 0)      { NEED_VALUE(); o->fault = atof(v); }
        else if (strcmp(a, "--band-low") == 0)   { NEED_VALUE(); o->band_low = atof(v); }
        else if (strcmp(a, "--band-high") == 0)  { NEED_VALUE(); o->band_high = atof(v); }
        else if (strcmp(a, "--seed") == 0)       { NEED_VALUE(); o->seed = strtoul(v, NULL, 10); }
        else if (strcmp(a, "--drop") == 0) {
            NEED_VALUE();
            if (!parse_drops(o, v)) {
                fprintf(stderr, "p04: --drop wants a comma separated list of sequence numbers\n");
                return false;
            }
        } else {
            fprintf(stderr, "p04: unknown option %s\n", a);
            return false;
        }
        #undef NEED_VALUE
    }
    return true;
}

static bool require(const char *what, const char *value)
{
    if (value == NULL) {
        fprintf(stderr, "p04: %s is required\n", what);
        return false;
    }
    return true;
}

/* ---------------------------------------------------------------- commands */

static int cmd_synth(const opts *o)
{
    if (!require("--kind", o->kind) || !require("--out", o->out))
        return 2;
    p04_synth_kind kind;
    if (p04_synth_kind_parse(o->kind, &kind) != P04_OK) {
        fprintf(stderr, "p04: --kind must be idle, touch or speech\n");
        return 2;
    }
    if (o->len <= 0 || o->len > P04_BLOCK_MAX) {
        fprintf(stderr, "p04: --len must be between 1 and %d\n", P04_BLOCK_MAX);
        return 2;
    }

    /* One count is a quarter of a milli-g, so one gravity is 4000 counts and
     * the invariant check has something to check against. */
    const float g_per_count = 1.0f / 4000.0f;

    p04_capture cap;
    p04_status s = p04_capture_create(&cap, o->out, (uint16_t)o->len,
                                      (float)o->rate, g_per_count);
    if (s != P04_OK) {
        fprintf(stderr, "p04: %s: %s\n", o->out, p04_strerror(s));
        return 1;
    }

    static p04_block b;
    uint32_t rng = (uint32_t)(o->seed ? o->seed : 1u);
    uint32_t written = 0;
    for (long i = 0; i < o->blocks; i++) {
        uint32_t seq = (uint32_t)i;
        /* A dropped block is generated and discarded rather than skipped, so
         * that the random stream stays in step and the only difference from
         * an undropped capture is the hole itself. */
        p04_synth_block(&b, kind, seq, (uint16_t)o->len, (float)o->rate, &rng);
        if (dropped(o, seq))
            continue;
        s = p04_capture_write(&cap, &b);
        if (s != P04_OK) {
            fprintf(stderr, "p04: %s: %s\n", o->out, p04_strerror(s));
            p04_capture_close(&cap);
            return 1;
        }
        written++;
    }
    s = p04_capture_close(&cap);
    if (s != P04_OK) {
        fprintf(stderr, "p04: %s: %s\n", o->out, p04_strerror(s));
        return 1;
    }
    printf("wrote %s: %u blocks of %ld samples at %.0f Hz, kind %s\n",
           o->out, written, o->len, o->rate, o->kind);
    if (o->n_drops > 0)
        printf("  %zu sequence numbers deliberately absent\n", o->n_drops);
    return 0;
}

/* Shared by every command that reads a capture and a book. */
static int open_pair(const opts *o, p04_capture *cap, p04_thresholds *t, bool need_book)
{
    if (!require("--source", o->source))
        return 2;
    p04_status s = p04_capture_open(cap, o->source);
    if (s != P04_OK) {
        fprintf(stderr, "p04: %s: %s\n", o->source, p04_strerror(s));
        return 1;
    }
    p04_thresholds_default(t);
    if (need_book) {
        if (!require("--book", o->book)) {
            p04_capture_close(cap);
            return 2;
        }
        s = p04_labbook_read(o->book, t);
        if (s != P04_OK) {
            fprintf(stderr, "p04: %s: %s\n", o->book, p04_strerror(s));
            p04_capture_close(cap);
            return 1;
        }
    }
    return 0;
}

/* The frequency axis check. The band edges in the book were turned into bins
 * at the rate the baseline was taken at. Applying them to a capture that
 * arrived at a different rate points them at different frequencies, so the
 * answer would be confident and wrong. */
static bool axis_agrees(const p04_capture *cap, const p04_thresholds *t)
{
    p04_timing timing;
    p04_timing_check(&timing, t->rate_hz, cap->rate_hz, RATE_TOLERANCE);
    if (!timing.trustworthy) {
        fprintf(stderr,
                "p04: refusing to classify. The lab book was written at %.0f Hz "
                "and this capture arrived at %.0f Hz, a %.1f per cent difference. "
                "The band edges would point at the wrong frequencies.\n",
                (double)timing.declared_hz, (double)timing.measured_hz,
                100.0 * (double)timing.error_fraction);
        return false;
    }
    return true;
}

static void print_transport(const p04_transport *tr)
{
    printf("  stream: %u blocks received, %u expected, %u missing, "
           "%u duplicated, %u out of order\n",
           tr->received, tr->expected, tr->gaps, tr->duplicates, tr->reorders);
}

static int cmd_baseline(const opts *o)
{
    if (!require("--out", o->out))
        return 2;
    p04_capture cap;
    p04_thresholds t;
    int rc = open_pair(o, &cap, &t, false);
    if (rc != 0)
        return rc;

    t.warn_ratio = (float)o->warn;
    t.fault_ratio = (float)o->fault;
    t.band_low_hz = (float)o->band_low;
    t.band_high_hz = (float)o->band_high;
    t.rate_hz = cap.rate_hz;
    t.g_per_count = cap.g_per_count;
    snprintf(t.mounting, sizeof t.mounting, "%s", o->mounting);

    if (t.fault_ratio < t.warn_ratio) {
        fprintf(stderr, "p04: --fault must not be below --warn\n");
        p04_capture_close(&cap);
        return 2;
    }

    /* The baseline is measured with the baseline unset, so every block scores
     * NORMAL and only the energies matter. */
    p04_result r;
    p04_status s = p04_run(&cap, &t, &r);
    p04_capture_close(&cap);
    if (s != P04_OK) {
        fprintf(stderr, "p04: %s: %s\n", o->source, p04_strerror(s));
        return 1;
    }
    if (r.count == 0) {
        fprintf(stderr, "p04: %s holds no blocks, so there is no baseline to take\n",
                o->source);
        return 1;
    }

    double sum = 0.0;
    for (uint32_t i = 0; i < r.count; i++)
        sum += (double)r.readings[i].vib_energy;
    t.baseline = (float)(sum / (double)r.count);

    s = p04_labbook_write(o->out, &t);
    if (s != P04_OK) {
        fprintf(stderr, "p04: %s: %s\n", o->out, p04_strerror(s));
        return 1;
    }
    printf("baseline %.6g over %u blocks, band %.0f to %.0f Hz at %.0f Hz\n",
           (double)t.baseline, r.count, (double)t.band_low_hz,
           (double)t.band_high_hz, (double)t.rate_hz);
    printf("  mounting: %s\n", t.mounting);
    print_transport(&r.transport);
    if (!p04_transport_whole(&r.transport))
        printf("  note: this baseline was taken over a stream with holes in it\n");
    printf("wrote %s\n", o->out);
    return 0;
}

static int analyse_common(const opts *o, bool per_block)
{
    p04_capture cap;
    p04_thresholds t;
    int rc = open_pair(o, &cap, &t, true);
    if (rc != 0)
        return rc;

    if (!axis_agrees(&cap, &t)) {
        p04_capture_close(&cap);
        return 1;
    }
    if (!(t.baseline > 0.0f)) {
        fprintf(stderr, "p04: %s carries no baseline, so nothing can be classified\n",
                o->book);
        p04_capture_close(&cap);
        return 1;
    }

    p04_result r;
    p04_status s = p04_run(&cap, &t, &r);
    p04_capture_close(&cap);
    if (s != P04_OK) {
        fprintf(stderr, "p04: %s: %s\n", o->source, p04_strerror(s));
        return 1;
    }
    if (r.count == 0) {
        fprintf(stderr, "p04: %s holds no blocks\n", o->source);
        return 1;
    }

    if (per_block) {
        for (uint32_t i = 0; i < r.count; i++) {
            const p04_reading *x = &r.readings[i];
            printf("seq %-6u %-7s ratio %8.3f  band %12.6g  mic rms %9.2f\n",
                   x->seq, p04_state_name(x->state), (double)x->ratio,
                   (double)x->vib_energy, (double)x->mic_rms);
        }
    }

    printf("%s over %u blocks\n", p04_state_name(p04_result_worst(&r)), r.count);
    printf("  mean ratio %.3f, mean mic rms %.2f\n",
           (double)p04_result_mean_ratio(&r), (double)p04_result_mean_mic_rms(&r));
    print_transport(&r.transport);
    if (r.overflowed)
        printf("  note: more blocks than this build keeps readings for\n");
    if (!p04_transport_whole(&r.transport))
        printf("  note: the stream has holes, so the spectrum is partly a picture of them\n");
    return 0;
}

static int cmd_analyse(const opts *o) { return analyse_common(o, false); }
static int cmd_replay(const opts *o)  { return analyse_common(o, true); }

static int cmd_transport(const opts *o)
{
    p04_capture cap;
    p04_thresholds t;
    int rc = open_pair(o, &cap, &t, false);
    if (rc != 0)
        return rc;

    /* Only the sequence numbers are wanted, so the blocks are read and their
     * contents ignored. */
    p04_transport tr;
    p04_transport_init(&tr);
    static p04_block b;
    for (;;) {
        p04_status s = p04_capture_read(&cap, &b);
        if (s == P04_E_EOF)
            break;
        if (s != P04_OK) {
            fprintf(stderr, "p04: %s: %s\n", o->source, p04_strerror(s));
            p04_capture_close(&cap);
            return 1;
        }
        p04_transport_see(&tr, b.seq);
    }
    p04_capture_close(&cap);

    print_transport(&tr);
    if (!p04_transport_whole(&tr)) {
        fprintf(stderr, "p04: the stream is not whole\n");
        return 1;
    }
    printf("the stream is whole\n");
    return 0;
}

static int cmd_validate(const opts *o)
{
    p04_capture cap;
    p04_thresholds t;
    int rc = open_pair(o, &cap, &t, true);
    if (rc != 0)
        return rc;

    float g_per_count = cap.g_per_count != 0.0f ? cap.g_per_count : t.g_per_count;
    if (g_per_count == 0.0f) {
        fprintf(stderr, "p04: neither the capture nor the lab book says what a "
                        "count is worth in g, so gravity cannot be checked\n");
        p04_capture_close(&cap);
        return 1;
    }

    static p04_block b;
    double sum = 0.0;
    uint32_t n = 0;
    for (;;) {
        p04_status s = p04_capture_read(&cap, &b);
        if (s == P04_E_EOF)
            break;
        if (s != P04_OK) {
            fprintf(stderr, "p04: %s: %s\n", o->source, p04_strerror(s));
            p04_capture_close(&cap);
            return 1;
        }
        sum += (double)p04_dc_level_g(b.vib, b.len, g_per_count);
        n++;
    }
    p04_capture_close(&cap);

    if (n == 0) {
        fprintf(stderr, "p04: %s holds no blocks\n", o->source);
        return 1;
    }
    double mean = sum / (double)n;
    double off = mean >= 0.0 ? mean - 1.0 : mean + 1.0;
    if (off < 0.0)
        off = -off;

    printf("mean DC on the vibration channel %.4f g over %u blocks\n", mean, n);
    if (off > (double)GRAVITY_LIMIT_G) {
        fprintf(stderr,
                "p04: a probe at rest should read one gravity and this reads %.4f. "
                "The scale, the axis or the mounting is not what the lab book says.\n",
                mean);
        return 1;
    }
    printf("gravity is where it should be, within %.2f g\n", (double)GRAVITY_LIMIT_G);
    return 0;
}

/* ---------------------------------------------------------------- main */

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage(stderr);
        return 2;
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage(stdout);
        return 0;
    }

    opts o;
    opts_init(&o);
    if (!parse_args(&o, argc, argv))
        return 2;

    const char *cmd = argv[1];
    if (strcmp(cmd, "synth") == 0)     return cmd_synth(&o);
    if (strcmp(cmd, "baseline") == 0)  return cmd_baseline(&o);
    if (strcmp(cmd, "analyse") == 0)   return cmd_analyse(&o);
    if (strcmp(cmd, "replay") == 0)    return cmd_replay(&o);
    if (strcmp(cmd, "transport") == 0) return cmd_transport(&o);
    if (strcmp(cmd, "validate") == 0)  return cmd_validate(&o);

    fprintf(stderr, "p04: unknown command %s\n", cmd);
    usage(stderr);
    return 2;
}
