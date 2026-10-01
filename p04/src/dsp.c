/* The transform, the window, and the band.
 *
 * Nothing here allocates and nothing here reads a file. It takes int16_t in
 * and gives float out, which keeps the one place where a count becomes a
 * number small enough to check.
 */

#include "p04.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* The transform needs two scratch arrays and they are the largest thing in the
 * program. They are file-scope rather than stack so that a block length of
 * 4096 does not put 32 kB on the stack of whatever called in, which on a
 * thread with a small stack is a fault that looks like data corruption.
 *
 * This makes the spectrum functions not reentrant. That is stated here rather
 * than discovered later: one stream, one thread, by design. */
static float g_re[P04_BLOCK_MAX];
static float g_im[P04_BLOCK_MAX];

void p04_window_hann(p04_window *win, uint16_t len)
{
    if (len == 0 || len > P04_BLOCK_MAX) {
        win->len = 0;
        win->coherent_gain = 1.0f;
        return;
    }
    win->len = len;
    double sum = 0.0;
    for (uint16_t i = 0; i < len; i++) {
        /* The periodic form, divisor len rather than len-1, because the signal
         * is a stream rather than a single record and consecutive blocks are
         * meant to join without a step. */
        double w = 0.5 - 0.5 * cos((2.0 * M_PI * (double)i) / (double)len);
        win->w[i] = (float)w;
        sum += w;
    }
    win->coherent_gain = (float)(sum / (double)len);
    if (win->coherent_gain <= 0.0f)
        win->coherent_gain = 1.0f;
}

static bool is_power_of_two(uint16_t n)
{
    return n != 0 && (n & (uint16_t)(n - 1)) == 0;
}

p04_status p04_fft(float *re, float *im, uint16_t len)
{
    if (re == NULL || im == NULL)
        return P04_E_ARG;
    if (len > P04_BLOCK_MAX || !is_power_of_two(len))
        return P04_E_RANGE;
    if (len == 1)
        return P04_OK;

    /* Bit reversal, in place. */
    for (uint16_t i = 1, j = 0; i < len; i++) {
        uint16_t bit = (uint16_t)(len >> 1);
        for (; (j & bit) != 0; bit = (uint16_t)(bit >> 1))
            j = (uint16_t)(j ^ bit);
        j = (uint16_t)(j ^ bit);
        if (i < j) {
            float tr = re[i]; re[i] = re[j]; re[j] = tr;
            float ti = im[i]; im[i] = im[j]; im[j] = ti;
        }
    }

    /* Danielson and Lanczos, iteratively. The twiddle is recomputed per stage
     * rather than tabulated: a table costs memory this program does not have
     * to spend, and at 2048 points the cosine calls are not the cost. */
    for (uint16_t span = 2; span <= len; span = (uint16_t)(span << 1)) {
        double ang = -2.0 * M_PI / (double)span;
        float wr = (float)cos(ang);
        float wi = (float)sin(ang);
        for (uint16_t start = 0; start < len; start = (uint16_t)(start + span)) {
            float cr = 1.0f, ci = 0.0f;
            for (uint16_t k = 0; k < span / 2; k++) {
                uint16_t a = (uint16_t)(start + k);
                uint16_t b = (uint16_t)(a + span / 2);
                float xr = re[b] * cr - im[b] * ci;
                float xi = re[b] * ci + im[b] * cr;
                re[b] = re[a] - xr;
                im[b] = im[a] - xi;
                re[a] += xr;
                im[a] += xi;
                float nr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = nr;
            }
        }
    }
    return P04_OK;
}

p04_status p04_spectrum(const int16_t *x, uint16_t len, const p04_window *win,
                        float *out, uint16_t *out_bins)
{
    if (x == NULL || win == NULL || out == NULL)
        return P04_E_ARG;
    if (len == 0 || len > P04_BLOCK_MAX || !is_power_of_two(len))
        return P04_E_RANGE;
    if (win->len != len)
        return P04_E_ARG;

    for (uint16_t i = 0; i < len; i++) {
        g_re[i] = (float)x[i] * win->w[i];
        g_im[i] = 0.0f;
    }
    p04_status s = p04_fft(g_re, g_im, len);
    if (s != P04_OK)
        return s;

    uint16_t bins = (uint16_t)(len / 2 + 1);
    for (uint16_t k = 0; k < bins; k++)
        out[k] = sqrtf(g_re[k] * g_re[k] + g_im[k] * g_im[k]);
    if (out_bins != NULL)
        *out_bins = bins;
    return P04_OK;
}

p04_status p04_band_bins(uint16_t n_bins, float rate_hz, float low_hz, float high_hz,
                         uint16_t *first, uint16_t *last)
{
    if (first == NULL || last == NULL)
        return P04_E_ARG;
    if (n_bins < 2 || rate_hz <= 0.0f)
        return P04_E_ARG;
    if (!(high_hz > low_hz) || low_hz < 0.0f)
        return P04_E_ARG;

    float nyquist = rate_hz / 2.0f;
    if (low_hz >= nyquist) {
        /* A band that starts above Nyquist cannot be measured. Returning an
         * empty sum here would read as silence, which is the wrong answer to
         * a question that should not have been asked. */
        return P04_E_RANGE;
    }

    float per_bin = nyquist / (float)(n_bins - 1);
    long lo = lroundf(low_hz / per_bin);
    long hi = lroundf(high_hz / per_bin);
    if (lo < 0) lo = 0;
    if (hi > (long)(n_bins - 1)) hi = (long)(n_bins - 1);
    if (hi < lo) hi = lo;
    *first = (uint16_t)lo;
    *last = (uint16_t)hi;
    return P04_OK;
}

p04_status p04_band_energy(const int16_t *x, uint16_t len, const p04_window *win,
                           float rate_hz, float low_hz, float high_hz, float *out)
{
    if (out == NULL)
        return P04_E_ARG;

    static float mag[P04_BLOCK_MAX / 2 + 1];
    uint16_t bins = 0;
    p04_status s = p04_spectrum(x, len, win, mag, &bins);
    if (s != P04_OK)
        return s;

    uint16_t first = 0, last = 0;
    s = p04_band_bins(bins, rate_hz, low_hz, high_hz, &first, &last);
    if (s != P04_OK)
        return s;

    double sum = 0.0;
    for (uint16_t k = first; k <= last; k++)
        sum += (double)mag[k] * (double)mag[k];

    /* Divide out the window's coherent gain and the block length so that two
     * captures taken with different block lengths are still comparable. */
    double norm = (double)len * (double)win->coherent_gain;
    *out = (float)(sum / (norm * norm));
    return P04_OK;
}

float p04_rms(const int16_t *x, uint16_t len)
{
    if (x == NULL || len == 0)
        return 0.0f;
    double sum = 0.0;
    for (uint16_t i = 0; i < len; i++)
        sum += (double)x[i] * (double)x[i];
    return (float)sqrt(sum / (double)len);
}
