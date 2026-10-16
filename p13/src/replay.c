/* Re-running a recorded log, and judging one.
 *
 * The replay ticks at one hertz between records, which is what the live
 * loop does, so a replay of a run reaches the same transitions at the same
 * times as the run did. Without that the two would differ by up to a second
 * at every flip and the log would stop being evidence of what happened.
 */

#include "p13.h"

#include <stdlib.h>
#include <string.h>

#define P13_EVENTS_MAX 4096
#define P13_TRANSITIONS_MAX 64
#define P13_TICK_S 1.0

/* Read "<time> record <tag>" or "<time> state <NAME> [age <seconds>]". */
typedef struct {
    double t;
    char kind[8];      /* record or state */
    char what[16];     /* the tag, or the state name */
    double age;
    bool has_age;
} p13_line;

static bool parse_line(const char *s, p13_line *out)
{
    if (s == NULL || out == NULL || s[0] == '#')
        return false;
    memset(out, 0, sizeof *out);

    char *end = NULL;
    out->t = strtod(s, &end);
    if (end == s)
        return false;
    while (*end == ' ')
        end++;

    const char *kind = end;
    size_t n = 0;
    while (kind[n] != '\0' && kind[n] != ' ')
        n++;
    if (n == 0 || n >= sizeof out->kind)
        return false;
    memcpy(out->kind, kind, n);
    out->kind[n] = '\0';

    const char *rest = kind + n;
    while (*rest == ' ')
        rest++;
    size_t m = 0;
    while (rest[m] != '\0' && rest[m] != ' ')
        m++;
    if (m == 0 || m >= sizeof out->what)
        return false;
    memcpy(out->what, rest, m);
    out->what[m] = '\0';

    const char *age = strstr(rest, " age ");
    if (age != NULL) {
        out->age = atof(age + 5);
        out->has_age = true;
    }
    return true;
}

p13_status p13_replay(const char *path, double window_s, p13_supervisor *out)
{
    if (path == NULL || out == NULL)
        return P13_E_ARG;

    linereader r;
    if (linereader_open(&r, path) != LINEREADER_OK)
        return P13_E_IO;

    p13_init(out, window_s);
    double clock = 0.0;
    bool started = false;

    for (;;) {
        const char *line = NULL;
        size_t len = 0;
        if (linereader_line(&r, &line, &len) != LINEREADER_OK)
            break;

        p13_line ev;
        if (!parse_line(line, &ev))
            continue;
        if (strcmp(ev.kind, "record") != 0)
            continue;   /* the state lines are the log's own output */

        if (!started) {
            clock = ev.t;
            started = true;
        }
        /* The ticks the live loop would have run between the two records. */
        while (clock + P13_TICK_S <= ev.t) {
            clock += P13_TICK_S;
            p13_on_tick(out, clock);
        }
        clock = ev.t;
        p13_on_record(out, ev.t, ev.what, NULL);
    }

    linereader_close(&r);
    return P13_OK;
}

/* ---------------------------------------------------------------- the gate */

static double mean_interval(const double *t, uint32_t n)
{
    if (n < 2)
        return -1.0;
    return (t[n - 1] - t[0]) / (double)(n - 1);
}

p13_status p13_verify(const char *path, double window_s, p13_verdict *v)
{
    if (path == NULL || v == NULL)
        return P13_E_ARG;
    memset(v, 0, sizeof *v);
    v->worst_flip_lateness = 0.0;
    v->backup_interval_before = -1.0;
    v->backup_interval_after = -1.0;

    static double primary[P13_EVENTS_MAX];
    static double backup[P13_EVENTS_MAX];
    static double flip_at[P13_TRANSITIONS_MAX];
    static double back_at[P13_TRANSITIONS_MAX];
    uint32_t np = 0, nb = 0, nf = 0, nr = 0;

    linereader r;
    if (linereader_open(&r, path) != LINEREADER_OK)
        return P13_E_IO;

    bool seen_primary_state = false;
    for (;;) {
        const char *line = NULL;
        size_t len = 0;
        if (linereader_line(&r, &line, &len) != LINEREADER_OK)
            break;

        p13_line ev;
        if (!parse_line(line, &ev))
            continue;

        if (strcmp(ev.kind, "record") == 0) {
            if (strcmp(ev.what, P13_PRIMARY_TAG) == 0 && np < P13_EVENTS_MAX)
                primary[np++] = ev.t;
            else if (strcmp(ev.what, P13_BACKUP_TAG) == 0 && nb < P13_EVENTS_MAX)
                backup[nb++] = ev.t;
        } else if (strcmp(ev.kind, "state") == 0) {
            if (strcmp(ev.what, "FAILOVER") == 0) {
                if (seen_primary_state && nf < P13_TRANSITIONS_MAX)
                    flip_at[nf++] = ev.t;
            } else if (strcmp(ev.what, "PRIMARY") == 0) {
                if (seen_primary_state && nf > 0 && nr < P13_TRANSITIONS_MAX)
                    back_at[nr++] = ev.t;
                seen_primary_state = true;
            }
        }
    }
    linereader_close(&r);

    v->primary_records = np;
    v->backup_records = nb;
    v->both_sources = (np > 0 && nb > 0);
    v->flipped = (nf > 0);
    v->returned = (nr > 0);

    /* Each fall must be no later than the window after the last primary
     * record before it, allowing the one second the tick loop costs. */
    v->flip_in_window = (nf > 0);
    for (uint32_t i = 0; i < nf; i++) {
        double last = -1.0;
        for (uint32_t j = 0; j < np; j++)
            if (primary[j] <= flip_at[i])
                last = primary[j];
        if (last < 0.0) {
            v->flip_in_window = false;
            continue;
        }
        double lateness = (flip_at[i] - last) - window_s;
        if (lateness > v->worst_flip_lateness)
            v->worst_flip_lateness = lateness;
        if (lateness > P13_TICK_S)
            v->flip_in_window = false;
    }

    /* Each return must land on a primary record rather than on a timer. */
    v->return_on_record = (nr > 0);
    for (uint32_t i = 0; i < nr; i++) {
        bool on_record = false;
        for (uint32_t j = 0; j < np; j++) {
            double d = back_at[i] - primary[j];
            if (d < 0.0)
                d = -d;
            if (d < 0.001) {
                on_record = true;
                break;
            }
        }
        if (!on_record)
            v->return_on_record = false;
    }

    /* The backup keeps its own rhythm through the transitions, because the
     * two are not connected. Judged only when there are enough records on
     * each side of the first fall to have a rhythm at all. */
    if (nf > 0 && nb >= 4) {
        uint32_t split = 0;
        while (split < nb && backup[split] < flip_at[0])
            split++;
        if (split >= 2 && nb - split >= 2) {
            v->backup_interval_before = mean_interval(backup, split);
            v->backup_interval_after = mean_interval(backup + split, nb - split);
            double a = v->backup_interval_before;
            double b = v->backup_interval_after;
            double diff = (a > b) ? a - b : b - a;
            v->backup_rhythm_kept = (a > 0.0 && diff <= a * 0.10);
        }
    }
    return P13_OK;
}

bool p13_verdict_passed(const p13_verdict *v)
{
    if (v == NULL)
        return false;
    return v->both_sources && v->flipped && v->returned &&
           v->flip_in_window && v->return_on_record && v->backup_rhythm_kept;
}

void p13_verdict_print(const p13_verdict *v, FILE *out)
{
    if (v == NULL || out == NULL)
        return;

    fprintf(out, "sources   %s  %lu primary records, %lu backup\n",
            v->both_sources ? "pass" : "FAIL",
            (unsigned long)v->primary_records, (unsigned long)v->backup_records);
    fprintf(out, "flip      %s\n", v->flipped
            ? "pass  the state fell to FAILOVER"
            : "FAIL  the state never fell, so the antenna test is not in this log");
    fprintf(out, "return    %s\n", v->returned
            ? "pass  and came back to PRIMARY"
            : "FAIL  it never came back, so the second half was not demonstrated");
    fprintf(out, "in window %s  worst fall was %.3f s later than the window\n",
            v->flip_in_window ? "pass" : "FAIL", v->worst_flip_lateness);
    fprintf(out, "on record %s  every return landed on a primary record\n",
            v->return_on_record ? "pass" : "FAIL");

    if (v->backup_interval_before < 0.0)
        fprintf(out, "rhythm    FAIL  not enough backup records either side of "
                     "the fall to judge whether the rhythm changed\n");
    else
        fprintf(out, "rhythm    %s  backup every %.1f s before the fall, "
                     "%.1f s after\n",
                v->backup_rhythm_kept ? "pass" : "FAIL",
                v->backup_interval_before, v->backup_interval_after);
}
