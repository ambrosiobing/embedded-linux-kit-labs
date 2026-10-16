/* Benches that were never knocked, written as captures that were.
 *
 * Two of the five exist to be refused. A sensor lying on its side reads one
 * gravity on a different axis and must produce no events at all, and free
 * fall reads zero, which is exactly as far from rest as two gravities and
 * must produce them. A threshold on one axis gets both of those wrong, and
 * in opposite directions.
 */

#include "p01.h"

#include <string.h>

static void encode(double g_value, uint8_t *lo, uint8_t *hi)
{
    double counts = g_value * P01_COUNTS_PER_G;
    if (counts > 32767.0)
        counts = 32767.0;
    if (counts < -32768.0)
        counts = -32768.0;
    int32_t v = (int32_t)(counts >= 0.0 ? counts + 0.5 : counts - 0.5);
    uint16_t u = (uint16_t)(v & 0xFFFF);
    *lo = (uint8_t)(u & 0xFFu);
    *hi = (uint8_t)(u >> 8);
}

static int write_sample(FILE *fp, double t, double ax, double ay, double az)
{
    uint8_t raw[6];
    encode(ax, &raw[0], &raw[1]);
    encode(ay, &raw[2], &raw[3]);
    encode(az, &raw[4], &raw[5]);
    return fprintf(fp, "%.3f %02x %02x %02x %02x %02x %02x\n", t,
                   raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]);
}

p01_status p01_scene_write(p01_scene scene, double seconds, double sample_hz,
                           const char *path)
{
    if (path == NULL || seconds <= 0.0 || sample_hz <= 0.0)
        return P01_E_ARG;

    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return P01_E_IO;

    int bad = fprintf(fp, "# seconds, then six bytes as the device returns "
                          "them, X low first\n");

    const double period = 1.0 / sample_hz;
    const uint32_t n = (uint32_t)(seconds * sample_hz);

    for (uint32_t i = 0; i < n && bad >= 0; i++) {
        const double t = (double)i * period;
        double ax = 0.0, ay = 0.0, az = 1.0;

        switch (scene) {
        case P01_SCENE_AT_REST:
            break;
        case P01_SCENE_ON_ITS_SIDE:
            /* The same gravity, on a different axis. Nothing is happening. */
            ax = 1.0;
            az = 0.0;
            break;
        case P01_SCENE_FREE_FALL:
            az = 0.0;
            break;
        case P01_SCENE_RINGING:
            az = 2.0;
            break;
        case P01_SCENE_ONE_KNOCK:
            /* Half a second of ringing after a single strike at one second,
             * decaying but staying well past the dead band throughout, which
             * is what makes the cool-off worth having. */
            if (t >= 1.0 && t < 1.5) {
                double decay = 1.0 - (t - 1.0) / 1.0;
                az = 1.0 + 1.5 * decay;
            }
            break;
        }
        bad = write_sample(fp, t, ax, ay, az);
    }

    if (fclose(fp) != 0 || bad < 0)
        return P01_E_IO;
    return P01_OK;
}
