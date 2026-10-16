/* Files the board did not produce, in the six shapes that matter.
 *
 * The good one is the reference. The other five are each a way a capture
 * can look finished and not be, and the chapter already names four of them
 * in prose: a hole, a repeated index, a short file, and a channel sitting
 * off its rail. The sixth is a sample past the input limit, which is a
 * wiring question that arrives disguised as a data question.
 */

#include "p05.h"

#include <string.h>

/* A small deterministic wobble, so a mean is a mean of something rather
 * than of one number repeated ten thousand times. */
static uint32_t mix(uint32_t x)
{
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

static double noise(uint32_t i, uint32_t salt)
{
    uint32_t h = mix(i ^ mix(salt * 2654435761u));
    /* Plus or minus about two millivolts, which is well inside the rail
     * tolerance and keeps the mean where it belongs. */
    return ((double)(h % 4001u) - 2000.0) / 1000000.0;
}

p05_status p05_scene_write(p05_scene scene, uint32_t rows, const char *path)
{
    if (path == NULL || rows == 0)
        return P05_E_ARG;

    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return P05_E_IO;

    int bad = fprintf(fp, "index,ch0,ch1\n");

    const uint32_t hole_at = rows / 2;
    const uint32_t hole_len = 7;
    const uint32_t repeat_at = rows / 3;
    const uint32_t over_at = rows / 4;

    uint32_t written = 0;
    for (uint32_t i = 0; i < rows && bad >= 0; i++) {
        if (scene == P05_SCENE_HOLE && i >= hole_at && i < hole_at + hole_len)
            continue;
        if (scene == P05_SCENE_SHORT && i >= rows - 25)
            break;

        double ch0 = P05_RAIL_3V3 + noise(i, 1);
        double ch1 = P05_RAIL_5V + noise(i, 2);

        if (scene == P05_SCENE_RAIL_LOW)
            ch0 -= 0.12;          /* off the rail by more than the tolerance */
        if (scene == P05_SCENE_OVER_RANGE && i == over_at)
            ch1 = P05_INPUT_LIMIT_V + 0.5;

        bad = fprintf(fp, "%lu,%.6f,%.6f\n", (unsigned long)i, ch0, ch1);
        written++;

        if (scene == P05_SCENE_REPEAT && i == repeat_at && bad >= 0) {
            /* The same index again, with its own sample. A row count alone
             * reads this file as complete. */
            bad = fprintf(fp, "%lu,%.6f,%.6f\n", (unsigned long)i,
                          ch0 + 0.001, ch1);
            written++;
        }
    }

    if (fclose(fp) != 0 || bad < 0)
        return P05_E_IO;
    (void)written;
    return P05_OK;
}
