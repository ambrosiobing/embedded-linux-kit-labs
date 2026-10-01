/* Thresholds, classification, the gravity invariant, the rate check, and the
 * pass over a capture that ties them together.
 *
 * The order in p04_run is the chapter's and it is deliberate. The sequence
 * numbers are accounted for first and reported beside the result, because a
 * spectrum computed over a stream with holes in it is a picture of the holes.
 * A run that classifies confidently over a stream that lost a third of its
 * blocks has answered a question nobody asked.
 */

#include "p04.h"

#include <math.h>
#include <string.h>

const char *p04_state_name(p04_state s)
{
    switch (s) {
    case P04_NORMAL:  return "NORMAL";
    case P04_WARNING: return "WARNING";
    case P04_FAULT:   return "FAULT";
    }
    return "UNKNOWN";
}

void p04_thresholds_default(p04_thresholds *t)
{
    if (t == NULL)
        return;
    memset(t, 0, sizeof *t);
    t->baseline = 0.0f;
    t->warn_ratio = 1.5f;
    t->fault_ratio = 3.0f;
    t->band_low_hz = P04_BAND_LOW_DEFAULT;
    t->band_high_hz = P04_BAND_HIGH_DEFAULT;
    t->rate_hz = P04_RATE_DEFAULT;
    t->g_per_count = 0.0f;
    snprintf(t->mounting, sizeof t->mounting, "%s", "unrecorded");
}

p04_state p04_classify(float energy, const p04_thresholds *t, float *ratio_out)
{
    float ratio = 0.0f;
    if (t != NULL && t->baseline > 0.0f)
        ratio = energy / t->baseline;
    if (ratio_out != NULL)
        *ratio_out = ratio;
    if (t == NULL)
        return P04_NORMAL;

    /* A baseline of zero means nobody measured one. Classifying against it
     * would make every block a fault, which reads as a broken machine rather
     * than as a missing step, so it stays NORMAL and the caller is told the
     * baseline is absent when the book is loaded. */
    if (!(t->baseline > 0.0f))
        return P04_NORMAL;
    if (ratio >= t->fault_ratio)
        return P04_FAULT;
    if (ratio >= t->warn_ratio)
        return P04_WARNING;
    return P04_NORMAL;
}

float p04_dc_level_g(const int16_t *x, uint16_t len, float g_per_count)
{
    if (x == NULL || len == 0 || g_per_count == 0.0f)
        return 0.0f;
    double sum = 0.0;
    for (uint16_t i = 0; i < len; i++)
        sum += (double)x[i];
    return (float)((sum / (double)len) * (double)g_per_count);
}

void p04_timing_check(p04_timing *t, float declared_hz, float measured_hz, float tolerance)
{
    if (t == NULL)
        return;
    t->declared_hz = declared_hz;
    t->measured_hz = measured_hz;
    t->error_fraction = 0.0f;
    t->trustworthy = false;
    if (declared_hz <= 0.0f || measured_hz <= 0.0f)
        return;
    t->error_fraction = fabsf(measured_hz - declared_hz) / declared_hz;
    t->trustworthy = t->error_fraction <= tolerance;
}

/* ---------------------------------------------------------------- pipeline */

void p04_result_init(p04_result *r)
{
    if (r == NULL)
        return;
    memset(r, 0, sizeof *r);
    p04_transport_init(&r->transport);
}

p04_status p04_run(p04_capture *cap, const p04_thresholds *t, p04_result *out)
{
    if (cap == NULL || t == NULL || out == NULL)
        return P04_E_ARG;

    p04_result_init(out);

    /* One window for the whole run. The block length comes from the capture
     * header, so a file whose blocks are not a power of two is refused by the
     * spectrum rather than silently mis-transformed. */
    static p04_window win;
    p04_window_hann(&win, cap->block_len);
    if (win.len != cap->block_len)
        return P04_E_RANGE;

    /* One block, reused. This is the reason the program has a fixed upper
     * block length: the buffer is here, once, rather than per iteration. */
    static p04_block b;

    for (;;) {
        p04_status s = p04_capture_read(cap, &b);
        if (s == P04_E_EOF)
            break;
        if (s != P04_OK)
            return s;

        p04_transport_see(&out->transport, b.seq);

        float energy = 0.0f;
        s = p04_band_energy(b.vib, b.len, &win, cap->rate_hz,
                            t->band_low_hz, t->band_high_hz, &energy);
        if (s != P04_OK)
            return s;

        float ratio = 0.0f;
        p04_state state = p04_classify(energy, t, &ratio);

        if (out->count < P04_READINGS_MAX) {
            p04_reading *r = &out->readings[out->count++];
            r->seq = b.seq;
            r->state = state;
            r->ratio = ratio;
            r->vib_energy = energy;
            r->mic_rms = p04_rms(b.mic, b.len);
        } else {
            /* Say so rather than drop quietly. A truncated run that reads like
             * a complete one is the failure this field exists to prevent. */
            out->overflowed = true;
        }
    }
    return P04_OK;
}

p04_state p04_result_worst(const p04_result *r)
{
    p04_state worst = P04_NORMAL;
    if (r == NULL)
        return worst;
    for (uint32_t i = 0; i < r->count; i++)
        if (r->readings[i].state > worst)
            worst = r->readings[i].state;
    return worst;
}

float p04_result_mean_ratio(const p04_result *r)
{
    if (r == NULL || r->count == 0)
        return 0.0f;
    double sum = 0.0;
    for (uint32_t i = 0; i < r->count; i++)
        sum += (double)r->readings[i].ratio;
    return (float)(sum / (double)r->count);
}

float p04_result_mean_mic_rms(const p04_result *r)
{
    if (r == NULL || r->count == 0)
        return 0.0f;
    double sum = 0.0;
    for (uint32_t i = 0; i < r->count; i++)
        sum += (double)r->readings[i].mic_rms;
    return (float)(sum / (double)r->count);
}
