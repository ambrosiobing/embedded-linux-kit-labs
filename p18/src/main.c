/* P18 on the command line.
 *
 * Exit status. Zero means the question was answered. Two means the command
 * line was wrong. One means either the question could not be answered, with
 * the reason on standard error, or the acceptance test was applied and
 * failed.
 *
 * That last part is a deliberate difference from the sibling lab, where a
 * classification of FAULT is still a zero exit because the program worked and
 * the machine did not. Here, verify is not a classifier. It is the lab's
 * acceptance test, it exists to be put in front of something, and a gate that
 * returns success when it did not pass is not a gate. sink and count report
 * and exit zero whatever the numbers say.
 */

#include "p18.h"

#include <stdlib.h>
#include <string.h>

static void usage(FILE *out)
{
    fprintf(out,
        "usage: p18 <command> [options]\n"
        "\n"
        "  synth   --out FILE [--samples N] [--start SEQ] [--seed N]\n"
        "          [--drop LIST] [--dup LIST] [--swap LIST]\n"
        "          [--garble LIST] [--long LIST]\n"
        "  sink    --source FILE [--out FILE] [--report FILE]\n"
        "          [--seconds S] [--rate HZ] [--max N]\n"
        "  count   --source FILE\n"
        "  verify  --report FILE [--max-loss PCT] [--rate-tolerance FRACTION]\n"
        "          [--allow-unmeasured-rate]\n"
        "\n"
        "A LIST is sequence numbers separated by commas. A source of - is\n"
        "standard input. Nothing here needs hardware: a character device is\n"
        "one more source, and a file is the same program's other half.\n");
}

/* ---------------------------------------------------------------- options */

typedef struct {
    const char *source;
    const char *out;
    const char *report;
    const char *drop;
    const char *dup;
    const char *swap;
    const char *garble;
    const char *longline;
    long samples;
    /* A sequence number rather than a count, so it is read as 32 bits
     * unsigned: a test that wants the wrap asks to start near the top of the
     * range, which is past the largest value a signed long holds on a host
     * where long is 32 bits. */
    uint32_t start;
    long seed;
    long max;
    double seconds;
    double rate;
    double max_loss;
    double rate_tolerance;
    bool allow_unmeasured_rate;
} opts;

static void opts_init(opts *o)
{
    memset(o, 0, sizeof *o);
    o->samples = 100;
    o->seed = 1;
    o->seconds = P18_SECONDS_DEFAULT;
    o->rate = P18_RATE_DEFAULT;
    o->max_loss = P18_MAX_LOSS_PCT_DEFAULT;
    o->rate_tolerance = P18_RATE_TOLERANCE_DEFAULT;
}

#define NEED_VALUE()                                                        \
    do {                                                                    \
        if (i + 1 >= argc) {                                                \
            fprintf(stderr, "p18: %s needs a value\n", a);                  \
            return false;                                                   \
        }                                                                   \
        v = argv[++i];                                                      \
    } while (0)

static bool parse_args(opts *o, int argc, char **argv)
{
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        const char *v = NULL;
        if (strcmp(a, "--source") == 0)              { NEED_VALUE(); o->source = v; }
        else if (strcmp(a, "--out") == 0)            { NEED_VALUE(); o->out = v; }
        else if (strcmp(a, "--report") == 0)         { NEED_VALUE(); o->report = v; }
        else if (strcmp(a, "--drop") == 0)           { NEED_VALUE(); o->drop = v; }
        else if (strcmp(a, "--dup") == 0)            { NEED_VALUE(); o->dup = v; }
        else if (strcmp(a, "--swap") == 0)           { NEED_VALUE(); o->swap = v; }
        else if (strcmp(a, "--garble") == 0)         { NEED_VALUE(); o->garble = v; }
        else if (strcmp(a, "--long") == 0)           { NEED_VALUE(); o->longline = v; }
        else if (strcmp(a, "--samples") == 0)        { NEED_VALUE(); o->samples = atol(v); }
        else if (strcmp(a, "--start") == 0)          { NEED_VALUE(); o->start = (uint32_t)strtoull(v, NULL, 10); }
        else if (strcmp(a, "--seed") == 0)           { NEED_VALUE(); o->seed = atol(v); }
        else if (strcmp(a, "--max") == 0)            { NEED_VALUE(); o->max = atol(v); }
        else if (strcmp(a, "--seconds") == 0)        { NEED_VALUE(); o->seconds = atof(v); }
        else if (strcmp(a, "--rate") == 0)           { NEED_VALUE(); o->rate = atof(v); }
        else if (strcmp(a, "--max-loss") == 0)       { NEED_VALUE(); o->max_loss = atof(v); }
        else if (strcmp(a, "--rate-tolerance") == 0) { NEED_VALUE(); o->rate_tolerance = atof(v); }
        else if (strcmp(a, "--allow-unmeasured-rate") == 0) o->allow_unmeasured_rate = true;
        else {
            fprintf(stderr, "p18: unknown option %s\n", a);
            return false;
        }
    }
    return true;
}

static bool require(const char *name, const char *value)
{
    if (value != NULL)
        return true;
    fprintf(stderr, "p18: %s is required\n", name);
    return false;
}

/* ---------------------------------------------------------------- synth */

static bool add_list(p18_synth *s, p18_fault_kind kind, const char *list)
{
    if (list == NULL)
        return true;
    const char *p = list;
    while (*p != '\0') {
        char *end = NULL;
        unsigned long long x = strtoull(p, &end, 10);
        if (end == p || x > 0xFFFFFFFFull) {
            fprintf(stderr, "p18: %s is not a list of sequence numbers\n", list);
            return false;
        }
        if (p18_synth_add_fault(s, kind, (uint32_t)x) != P18_OK) {
            fprintf(stderr, "p18: more than %d faults asked for\n", P18_FAULTS_MAX);
            return false;
        }
        p = end;
        if (*p == ',')
            p++;
        else if (*p != '\0') {
            fprintf(stderr, "p18: %s is not a list of sequence numbers\n", list);
            return false;
        }
    }
    return true;
}

static int cmd_synth(const opts *o)
{
    if (!require("--out", o->out))
        return 2;
    if (o->samples <= 0) {
        fprintf(stderr, "p18: --samples must be positive\n");
        return 2;
    }

    p18_synth s;
    p18_synth_init(&s);
    s.samples = (uint32_t)o->samples;
    s.start_seq = o->start;
    s.seed = (uint32_t)o->seed;

    if (!add_list(&s, P18_FAULT_DROP, o->drop))     return 2;
    if (!add_list(&s, P18_FAULT_DUP, o->dup))       return 2;
    if (!add_list(&s, P18_FAULT_SWAP, o->swap))     return 2;
    if (!add_list(&s, P18_FAULT_GARBLE, o->garble)) return 2;
    if (!add_list(&s, P18_FAULT_LONG, o->longline)) return 2;

    p18_status st = p18_synth_write(&s, o->out);
    if (st != P18_OK) {
        fprintf(stderr, "p18: %s: %s\n", o->out, p18_strerror(st));
        return 1;
    }
    printf("wrote %lu samples from %lu, %u faults, to %s\n",
           (unsigned long)s.samples, (unsigned long)s.start_seq,
           (unsigned)s.fault_count, o->out);
    return 0;
}

/* ---------------------------------------------------------------- reporting */

static void print_sequence(const p18_run *run)
{
    printf("samples %lu  gaps %lu  duplicates %lu  reorders %lu\n",
           (unsigned long)run->samples,
           (unsigned long)run->seq.gaps,
           (unsigned long)run->seq.duplicates,
           (unsigned long)run->seq.reorders);
    printf("sequence %lu to %lu, %lu expected, loss %.4f per cent\n",
           (unsigned long)run->seq.first_seq,
           (unsigned long)run->seq.last_seq,
           (unsigned long)run->seq.expected,
           p18_loss_percent(run));
    printf("lines refused: %lu malformed, %lu over-long, %lu blank\n",
           (unsigned long)run->malformed,
           (unsigned long)run->long_lines,
           (unsigned long)run->blank_lines);
}

static void print_timing(const p18_run *run)
{
    if (run->rate_measured) {
        printf("elapsed %.3f s, observed %.1f Hz against a nominal %.1f Hz\n",
               run->elapsed_s, run->observed_rate_hz, run->nominal_rate_hz);
    } else {
        printf("elapsed %.3f s, rate not measured: the source was not running\n"
               "in real time, so samples divided by that time would describe\n"
               "this program rather than the probe\n", run->elapsed_s);
    }
    if (run->deadline_hit)
        printf("stopped at the deadline\n");
    if (run->truncated)
        printf("stopped at the sample limit, the stream had more to give\n");
}

static int cmd_sink(const opts *o)
{
    if (!require("--source", o->source))
        return 2;

    p18_run run;
    p18_run_init(&run, o->rate);
    p18_status st = p18_run_capture(o->source, o->out, o->seconds,
                                    (uint32_t)(o->max > 0 ? o->max : 0), &run);
    if (st != P18_OK) {
        fprintf(stderr, "p18: %s: %s\n", o->source, p18_strerror(st));
        return 1;
    }

    print_sequence(&run);
    print_timing(&run);

    if (o->report != NULL) {
        st = p18_report_write(&run, o->report);
        if (st != P18_OK) {
            fprintf(stderr, "p18: %s: %s\n", o->report, p18_strerror(st));
            return 1;
        }
        printf("report written to %s\n", o->report);
    }
    return 0;
}

static int cmd_count(const opts *o)
{
    if (!require("--source", o->source))
        return 2;

    /* No deadline, no output file and no nominal rate, so nothing here can
     * produce a timing claim. Counting a capture that is already on the disk
     * says nothing about how fast it arrived, and this command does not
     * pretend otherwise. */
    p18_run run;
    p18_run_init(&run, 0.0);
    p18_status st = p18_run_capture(o->source, NULL, 0.0, 0, &run);
    if (st != P18_OK) {
        fprintf(stderr, "p18: %s: %s\n", o->source, p18_strerror(st));
        return 1;
    }
    print_sequence(&run);
    return 0;
}

static int cmd_verify(const opts *o)
{
    if (!require("--report", o->report))
        return 2;

    p18_run run;
    p18_status st = p18_report_read(&run, o->report);
    if (st != P18_OK) {
        fprintf(stderr, "p18: %s: %s\n", o->report, p18_strerror(st));
        return 1;
    }

    p18_gate g;
    p18_gate_init(&g);
    g.max_loss_pct = o->max_loss;
    g.rate_tolerance = o->rate_tolerance;
    g.require_rate = !o->allow_unmeasured_rate;

    p18_verdict v;
    if (p18_gate_apply(&g, &run, &v) != P18_OK) {
        fprintf(stderr, "p18: the report could not be read as a run\n");
        return 1;
    }

    printf("loss      %s  %.4f per cent of %lu expected, limit %.4f\n",
           v.loss_ok ? "pass" : "FAIL", v.loss_pct,
           (unsigned long)run.seq.expected, g.max_loss_pct);

    if (!run.rate_measured)
        printf("rate      %s  never measured, the source was not real time\n",
               v.rate_ok ? "pass" : "FAIL");
    else
        printf("rate      %s  %.1f Hz against %.1f Hz, %.2f of nominal, "
               "tolerance %.2f\n",
               v.rate_ok ? "pass" : "FAIL", run.observed_rate_hz,
               run.nominal_rate_hz, v.rate_ratio, g.rate_tolerance);

    printf("integrity %s  %lu malformed, %lu over-long\n",
           v.integrity_ok ? "pass" : "FAIL",
           (unsigned long)run.malformed, (unsigned long)run.long_lines);

    if (!p18_verdict_passed(&v)) {
        printf("\nThe acceptance test was not met. The three lines above are\n"
               "three different faults and the one that reads FAIL is the one\n"
               "to go after.\n");
        return 1;
    }
    printf("\nThe acceptance test was met on every count.\n");
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
    if (strcmp(cmd, "synth") == 0)  return cmd_synth(&o);
    if (strcmp(cmd, "sink") == 0)   return cmd_sink(&o);
    if (strcmp(cmd, "count") == 0)  return cmd_count(&o);
    if (strcmp(cmd, "verify") == 0) return cmd_verify(&o);

    fprintf(stderr, "p18: unknown command %s\n", cmd);
    usage(stderr);
    return 2;
}
