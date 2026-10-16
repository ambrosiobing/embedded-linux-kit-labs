/* The run report, and the gate that reads it.
 *
 * The report is a flat file of key and value, one per line, for the same
 * reason the sibling lab's lab book is: a capture is worth nothing without
 * the numbers that describe the run it came from, and a format that needs a
 * library to read is a format that will not be read on a board.
 *
 * The gate is three separate questions with three separate answers. Loss and
 * rate are different faults with different causes, and the integrity of the
 * capture is a third thing again: a recorder that threw lines away because
 * they did not fit its buffer has not produced a capture of the stream, no
 * matter what its loss figure says.
 */

#include "p18.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

p18_status p18_report_write(const p18_run *run, const char *path)
{
    if (run == NULL || path == NULL)
        return P18_E_ARG;
    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return P18_E_IO;

    int n = fprintf(fp,
        "# P18 run report. The loss and the rate are two measurements, not\n"
        "# one, and nothing here combines them into a single word.\n"
        "version=1\n"
        "samples=%lu\n"
        "received=%lu\n"
        "expected=%lu\n"
        "gaps=%lu\n"
        "duplicates=%lu\n"
        "reorders=%lu\n"
        "first_seq=%lu\n"
        "last_seq=%lu\n"
        "malformed=%lu\n"
        "long_lines=%lu\n"
        "blank_lines=%lu\n"
        "elapsed_s=%.9g\n"
        "nominal_rate_hz=%.9g\n"
        "observed_rate_hz=%.9g\n"
        "rate_measured=%d\n"
        "deadline_hit=%d\n"
        "truncated=%d\n",
        (unsigned long)run->samples,
        (unsigned long)run->seq.received,
        (unsigned long)run->seq.expected,
        (unsigned long)run->seq.gaps,
        (unsigned long)run->seq.duplicates,
        (unsigned long)run->seq.reorders,
        (unsigned long)run->seq.first_seq,
        (unsigned long)run->seq.last_seq,
        (unsigned long)run->malformed,
        (unsigned long)run->long_lines,
        (unsigned long)run->blank_lines,
        run->elapsed_s,
        run->nominal_rate_hz,
        run->observed_rate_hz,
        run->rate_measured ? 1 : 0,
        run->deadline_hit ? 1 : 0,
        run->truncated ? 1 : 0);

    if (fclose(fp) != 0 || n < 0)
        return P18_E_IO;
    return P18_OK;
}

static void trim(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' '))
        s[--n] = '\0';
}

static bool as_u32(const char *v, uint32_t *out)
{
    char *end = NULL;
    unsigned long long x = strtoull(v, &end, 10);
    if (end == v || x > 0xFFFFFFFFull)
        return false;
    *out = (uint32_t)x;
    return true;
}

p18_status p18_report_read(p18_run *run, const char *path)
{
    if (run == NULL || path == NULL)
        return P18_E_ARG;
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return P18_E_IO;

    p18_run_init(run, 0.0);

    char line[256];
    bool version_seen = false;
    while (fgets(line, (int)sizeof line, fp) != NULL) {
        if (line[0] == '#')
            continue;
        trim(line);
        char *eq = strchr(line, '=');
        if (eq == NULL)
            continue;
        *eq = '\0';
        const char *k = line;
        const char *v = eq + 1;

        if (strcmp(k, "version") == 0) {
            uint32_t ver = 0;
            if (!as_u32(v, &ver) || ver != 1) {
                fclose(fp);
                return P18_E_FORMAT;
            }
            version_seen = true;
        }
        else if (strcmp(k, "samples") == 0)      as_u32(v, &run->samples);
        else if (strcmp(k, "received") == 0)     as_u32(v, &run->seq.received);
        else if (strcmp(k, "expected") == 0)     as_u32(v, &run->seq.expected);
        else if (strcmp(k, "gaps") == 0)         as_u32(v, &run->seq.gaps);
        else if (strcmp(k, "duplicates") == 0)   as_u32(v, &run->seq.duplicates);
        else if (strcmp(k, "reorders") == 0)     as_u32(v, &run->seq.reorders);
        else if (strcmp(k, "first_seq") == 0)    as_u32(v, &run->seq.first_seq);
        else if (strcmp(k, "last_seq") == 0)     as_u32(v, &run->seq.last_seq);
        else if (strcmp(k, "malformed") == 0)    as_u32(v, &run->malformed);
        else if (strcmp(k, "long_lines") == 0)   as_u32(v, &run->long_lines);
        else if (strcmp(k, "blank_lines") == 0)  as_u32(v, &run->blank_lines);
        else if (strcmp(k, "elapsed_s") == 0)         run->elapsed_s = atof(v);
        else if (strcmp(k, "nominal_rate_hz") == 0)   run->nominal_rate_hz = atof(v);
        else if (strcmp(k, "observed_rate_hz") == 0)  run->observed_rate_hz = atof(v);
        else if (strcmp(k, "rate_measured") == 0)     run->rate_measured = (atoi(v) != 0);
        else if (strcmp(k, "deadline_hit") == 0)      run->deadline_hit = (atoi(v) != 0);
        else if (strcmp(k, "truncated") == 0)         run->truncated = (atoi(v) != 0);
    }

    if (fclose(fp) != 0)
        return P18_E_IO;
    if (!version_seen)
        return P18_E_FORMAT;
    run->seq.started = (run->seq.received > 0);
    return P18_OK;
}

void p18_gate_init(p18_gate *g)
{
    if (g == NULL)
        return;
    g->max_loss_pct = P18_MAX_LOSS_PCT_DEFAULT;
    g->rate_tolerance = P18_RATE_TOLERANCE_DEFAULT;
    g->require_rate = true;
}

p18_status p18_gate_apply(const p18_gate *g, const p18_run *run, p18_verdict *v)
{
    if (g == NULL || run == NULL || v == NULL)
        return P18_E_ARG;
    memset(v, 0, sizeof *v);

    v->loss_pct = p18_loss_percent(run);
    v->loss_ok = (v->loss_pct <= g->max_loss_pct);

    if (run->nominal_rate_hz > 0.0)
        v->rate_ratio = run->observed_rate_hz / run->nominal_rate_hz;
    else
        v->rate_ratio = 0.0;

    if (!run->rate_measured) {
        /* A rate that was never measured is not a rate that passed. Whether
         * that stops the run depends on what the operator asked for, and the
         * difference is one flag rather than a silent assumption. */
        v->rate_ok = !g->require_rate;
    } else {
        v->rate_ok = (fabs(v->rate_ratio - 1.0) <= g->rate_tolerance);
    }

    v->integrity_ok = (run->long_lines == 0 && run->malformed == 0);
    return P18_OK;
}

bool p18_verdict_passed(const p18_verdict *v)
{
    if (v == NULL)
        return false;
    return v->loss_ok && v->rate_ok && v->integrity_ok;
}
