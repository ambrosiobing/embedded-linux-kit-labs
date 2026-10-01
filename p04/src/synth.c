/* Synthetic stimuli, for work before the probe is wired.
 *
 * These are signals with the right shape. They are not a model of the probe,
 * of the machine, or of a room, and a threshold tuned against them is a
 * threshold tuned against them. The point is to exercise the pipeline and to
 * make the lab's own claim testable: that a case touch moves the vibration
 * band without moving the microphone, and a voice does the opposite.
 *
 * The generator is in C rather than in a script because it writes the capture
 * format, and one program owning both ends of a format is one fewer place for
 * the two to drift apart.
 *
 * The random source is a small deterministic generator carried in the caller's
 * variable rather than rand(), so that a capture is reproducible from its seed
 * on any machine and the determinism test is meaningful.
 */

#include "p04.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

p04_status p04_synth_kind_parse(const char *name, p04_synth_kind *out)
{
    if (name == NULL || out == NULL)
        return P04_E_ARG;
    if (strcmp(name, "idle") == 0)   { *out = P04_SYNTH_IDLE;   return P04_OK; }
    if (strcmp(name, "touch") == 0)  { *out = P04_SYNTH_TOUCH;  return P04_OK; }
    if (strcmp(name, "speech") == 0) { *out = P04_SYNTH_SPEECH; return P04_OK; }
    return P04_E_ARG;
}

/* xorshift32. Small, fast, and good enough for shaping noise. Not for
 * anything that needs to be unpredictable. */
static uint32_t next_rand(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

/* Uniform in minus one to one. */
static float noise(uint32_t *s)
{
    return ((float)(next_rand(s) >> 8) / (float)(1u << 23)) - 1.0f;
}

static int16_t clamp16(float v)
{
    if (v > 32767.0f)  return 32767;
    if (v < -32768.0f) return -32768;
    return (int16_t)lrintf(v);
}

void p04_synth_block(p04_block *b, p04_synth_kind kind, uint32_t seq,
                     uint16_t len, float rate_hz, uint32_t *rng)
{
    if (b == NULL || rng == NULL || len == 0 || len > P04_BLOCK_MAX)
        return;

    b->seq = seq;
    b->len = len;

    /* One gravity sits on the vibration channel as a constant, because a probe
     * at rest reads one g and the invariant check depends on it being there.
     * At 1/4000 g per count that is 4000 counts. */
    const float g_counts = 4000.0f;

    /* A tone inside the band for the touch case, and one below it for speech,
     * so that the two stimuli are separated by frequency rather than only by
     * amplitude. The phase continues across blocks so that consecutive blocks
     * join, which is what the periodic window assumes. */
    const float touch_hz = 3500.0f;
    const float speech_hz = 220.0f;

    for (uint16_t i = 0; i < len; i++) {
        double t = ((double)seq * (double)len + (double)i) / (double)rate_hz;
        float vib = g_counts + 12.0f * noise(rng);
        float mic = 30.0f * noise(rng);

        switch (kind) {
        case P04_SYNTH_IDLE:
            break;
        case P04_SYNTH_TOUCH:
            /* The case is struck: energy in the band, microphone unmoved. */
            vib += 900.0f * (float)sin(2.0 * M_PI * (double)touch_hz * t);
            break;
        case P04_SYNTH_SPEECH:
            /* A voice: the microphone rises by an order of magnitude and the
             * vibration band does not, which is the orthogonality the lab is
             * built to show. The small structural coupling is below the
             * band. */
            mic += 900.0f * (float)sin(2.0 * M_PI * (double)speech_hz * t);
            vib += 6.0f * (float)sin(2.0 * M_PI * (double)speech_hz * t);
            break;
        }
        b->vib[i] = clamp16(vib);
        b->mic[i] = clamp16(mic);
    }
}
