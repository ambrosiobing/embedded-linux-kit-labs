/* The decision, and the arithmetic of what it costs.
 *
 * One piece of state, one comparison, and a clock that arrives as an
 * argument so the suite can run ten minutes of a ringing table in no time
 * at all.
 */

#include "p01.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void p01_policy_init(p01_policy *p, double deadband_g, double cooloff_s)
{
    if (p == NULL)
        return;
    memset(p, 0, sizeof *p);
    p->deadband_g = (deadband_g > 0.0) ? deadband_g : P01_DEADBAND_G;
    p->cooloff_s = (cooloff_s >= 0.0) ? cooloff_s : P01_COOLOFF_S;
    p->last_event_at = -1.0;
    p->first_sample_at = -1.0;
}

bool p01_policy_offer(p01_policy *p, double now, const p01_sample *s)
{
    if (p == NULL || s == NULL)
        return false;

    p->samples++;
    if (!p->armed) {
        p->armed = true;
        p->first_sample_at = now;
    }
    p->last_sample_at = now;

    /* The distance from one gravity, on the magnitude of the vector. Not on
     * an axis: a sensor lying on its side still reads one gravity, and a
     * threshold on one axis calls being picked up an event. Free fall is as
     * far from rest as two gravities is, and in the opposite direction,
     * which this comparison gets right for free. */
    if (fabs(s->magnitude - 1.0) <= p->deadband_g)
        return false;

    p->over_threshold++;

    if (now < p->next_allowed) {
        p->suppressed++;
        return false;
    }

    p->events++;
    p->last_event_at = now;
    p->next_allowed = now + p->cooloff_s;
    return true;
}

/* ---------------------------------------------------------------- budget */

p01_status p01_budget_make(p01_budget *b, double sample_hz, double cooloff_s,
                           double fsync_s, uint32_t record_bytes, double hours)
{
    if (b == NULL)
        return P01_E_ARG;
    if (sample_hz <= 0.0 || hours <= 0.0 || cooloff_s <= 0.0 || fsync_s <= 0.0)
        return P01_E_ARG;

    memset(b, 0, sizeof *b);
    b->sample_hz = sample_hz;
    b->cooloff_s = cooloff_s;
    b->fsync_s = fsync_s;
    b->record_bytes = record_bytes;
    b->hours = hours;

    const double seconds = hours * 3600.0;
    b->samples = sample_hz * seconds;

    /* The worst case, which is the only one worth budgeting: a surface that
     * never stops ringing, so every sample is over the threshold. */
    b->events_without_cooloff = b->samples;
    b->events = seconds / cooloff_s;
    if (b->events > b->samples)
        b->events = b->samples;   /* a cool-off shorter than the sample period */

    b->bytes_to_flash = b->events * (double)record_bytes;
    b->bytes_without_cooloff = b->events_without_cooloff * (double)record_bytes;
    b->fsyncs = seconds / fsync_s;
    b->fsyncs_per_event = (b->events > 0.0) ? b->fsyncs / b->events : 0.0;
    b->compression = (b->events > 0.0)
                   ? b->events_without_cooloff / b->events : 0.0;
    return P01_OK;
}

void p01_budget_print(const p01_budget *b, FILE *out)
{
    if (b == NULL || out == NULL)
        return;

    fprintf(out, "a surface ringing for %.1f hours at %.0f Hz is %.0f samples\n",
            b->hours, b->sample_hz, b->samples);
    fprintf(out, "without the cool-off that is %.0f events and %.1f MB written\n",
            b->events_without_cooloff, b->bytes_without_cooloff / 1048576.0);
    fprintf(out, "with a %.1f s cool-off it is %.0f events and %.2f MB, "
                 "a factor of %.0f\n",
            b->cooloff_s, b->events, b->bytes_to_flash / 1048576.0,
            b->compression);
    fprintf(out, "%.0f fsyncs at one every %.0f s, which is %.3f per event\n",
            b->fsyncs, b->fsync_s, b->fsyncs_per_event);

    if (b->fsyncs_per_event < 1.0)
        fprintf(out, "fewer than one fsync per event, which is what "
                     "committing on a timer rather than on every sample "
                     "means in numbers\n");
    else
        fprintf(out, "more than one fsync per event: the timer is shorter "
                     "than the cool-off, so the flash is being committed to "
                     "when nothing has been written\n");
}

/* ---------------------------------------------------------------- replay */

p01_status p01_replay(const char *path, double deadband_g, double cooloff_s,
                      p01_run *out)
{
    if (path == NULL || out == NULL)
        return P01_E_ARG;

    memset(out, 0, sizeof *out);
    p01_policy_init(&out->policy, deadband_g, cooloff_s);

    linereader r;
    if (linereader_open(&r, path) != LINEREADER_OK)
        return P01_E_IO;

    for (;;) {
        const char *line = NULL;
        size_t len = 0;
        if (linereader_line(&r, &line, &len) != LINEREADER_OK)
            break;
        if (line[0] == '#')
            continue;
        out->lines++;

        char *end = NULL;
        double t = strtod(line, &end);
        if (end == line) {
            out->malformed++;
            continue;
        }
        uint8_t raw[6];
        if (!p01_parse_bytes(end, raw)) {
            out->malformed++;
            continue;
        }

        p01_sample s;
        p01_decode(raw, &s);
        if (s.magnitude > out->peak_magnitude)
            out->peak_magnitude = s.magnitude;
        p01_policy_offer(&out->policy, t, &s);
    }

    linereader_close(&r);
    return P01_OK;
}

void p01_run_print(const p01_run *r, FILE *out)
{
    if (r == NULL || out == NULL)
        return;

    const p01_policy *p = &r->policy;
    fprintf(out, "%lu lines, %lu malformed, %lu samples decoded\n",
            (unsigned long)r->lines, (unsigned long)r->malformed,
            (unsigned long)p->samples);
    fprintf(out, "%lu samples past the dead band, %lu published, "
                 "%lu suppressed by the cool-off\n",
            (unsigned long)p->over_threshold, (unsigned long)p->events,
            (unsigned long)p->suppressed);
    fprintf(out, "peak magnitude %.3f g, over %.1f s of samples\n",
            r->peak_magnitude,
            (p->samples > 0) ? p->last_sample_at - p->first_sample_at : 0.0);
    if (p->over_threshold > 0 && p->events > 0)
        fprintf(out, "one publish for every %.1f samples that would have "
                     "published, which is the compression this lab is for\n",
                (double)p->over_threshold / (double)p->events);
}
