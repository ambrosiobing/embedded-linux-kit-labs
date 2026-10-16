/* The measured wall, and the threshold that follows from it.
 *
 * The chapter's instruction is "measure the threshold, do not guess it", and
 * the only way a program can hold anyone to that is to refuse to produce one
 * from a measurement that does not deserve the name. Two refusals do that
 * work here: a baseline from fewer than ten frames, and a baseline whose own
 * frames disagree by more than the stability the acceptance test asks for.
 *
 * The file is flat key and value, like the sibling labs' books, because a
 * format that needs a library to read is a format that will not be read on a
 * board.
 */

#include "p06.h"

#include <stdlib.h>
#include <string.h>

/* The accumulators live beside the book rather than in it, so that what is
 * written to the file is only what a later run needs. That makes this module
 * non-reentrant on purpose: one baseline is measured at a time, by one
 * process, standing at one placement, and a second concurrent baseline would
 * be two people measuring the same wall. */
static uint32_t g_sum[P06_ZONES];
static uint16_t g_min[P06_ZONES];
static uint16_t g_max[P06_ZONES];

void p06_book_init(p06_book *b)
{
    if (b == NULL)
        return;
    memset(b, 0, sizeof *b);
    memset(g_sum, 0, sizeof g_sum);
    for (uint16_t i = 0; i < P06_ZONES; i++) {
        g_min[i] = 0xFFFF;
        g_max[i] = 0;
    }
}

p06_status p06_book_add(p06_book *b, const p06_frame *f)
{
    if (b == NULL || f == NULL)
        return P06_E_ARG;

    for (uint16_t i = 0; i < P06_ZONES; i++) {
        /* A zone with no target has no wall in it. A baseline is taken
         * against a surface the sensor can see, so the whole frame is
         * refused rather than a hole being left in the reference. */
        if (f->mm[i] == 0)
            return P06_E_RANGE;
    }
    for (uint16_t i = 0; i < P06_ZONES; i++) {
        g_sum[i] += f->mm[i];
        if (f->mm[i] < g_min[i])
            g_min[i] = f->mm[i];
        if (f->mm[i] > g_max[i])
            g_max[i] = f->mm[i];
    }
    b->frames++;
    return P06_OK;
}

p06_status p06_book_finish(p06_book *b, uint16_t margin_mm, uint16_t stability_limit_mm)
{
    if (b == NULL)
        return P06_E_ARG;
    if (b->frames < P06_BASELINE_MIN_FRAMES)
        return P06_E_RANGE;

    b->margin_mm = margin_mm;
    b->worst_spread_mm = 0;
    b->nearest_wall_mm = 0;

    for (uint16_t i = 0; i < P06_ZONES; i++) {
        uint32_t mean = g_sum[i] / b->frames;
        b->wall_mm[i] = (uint16_t)mean;
        b->spread_mm[i] = (uint16_t)(g_max[i] - g_min[i]);
        if (b->spread_mm[i] > b->worst_spread_mm)
            b->worst_spread_mm = b->spread_mm[i];
        if (b->nearest_wall_mm == 0 || b->wall_mm[i] < b->nearest_wall_mm)
            b->nearest_wall_mm = b->wall_mm[i];
    }

    if (b->worst_spread_mm > stability_limit_mm)
        return P06_E_RANGE;

    /* Under the nearest wall by the margin. Taking it from the nearest zone
     * rather than the average means a wall that is not square to the sensor
     * cannot put part of itself below the threshold and read as a person. */
    if (b->nearest_wall_mm <= margin_mm)
        return P06_E_RANGE;
    b->threshold_mm = (uint16_t)(b->nearest_wall_mm - margin_mm);
    return P06_OK;
}

p06_status p06_book_write(const p06_book *b, const char *path)
{
    if (b == NULL || path == NULL)
        return P06_E_ARG;
    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return P06_E_IO;

    int n = fprintf(fp,
        "# P06 lab book. Every number below was measured with the field of\n"
        "# view empty, at the placement named at the end, and means nothing\n"
        "# at a different one.\n"
        "version=1\n"
        "frames=%lu\n"
        "nearest_wall_mm=%u\n"
        "threshold_mm=%u\n"
        "margin_mm=%u\n"
        "worst_spread_mm=%u\n"
        "placement=%s\n",
        (unsigned long)b->frames,
        (unsigned)b->nearest_wall_mm,
        (unsigned)b->threshold_mm,
        (unsigned)b->margin_mm,
        (unsigned)b->worst_spread_mm,
        b->placement);

    for (uint16_t i = 0; i < P06_ZONES && n >= 0; i++)
        n = fprintf(fp, "zone%02u=%u,%u\n", (unsigned)i,
                    (unsigned)b->wall_mm[i], (unsigned)b->spread_mm[i]);

    if (fclose(fp) != 0 || n < 0)
        return P06_E_IO;
    return P06_OK;
}

static void trim(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' '))
        s[--n] = '\0';
}

p06_status p06_book_read(p06_book *b, const char *path)
{
    if (b == NULL || path == NULL)
        return P06_E_ARG;
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return P06_E_IO;

    p06_book_init(b);

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
            if (atoi(v) != 1) {
                fclose(fp);
                return P06_E_FORMAT;
            }
            version_seen = true;
        }
        else if (strcmp(k, "frames") == 0)           b->frames = (uint32_t)atol(v);
        else if (strcmp(k, "nearest_wall_mm") == 0)  b->nearest_wall_mm = (uint16_t)atoi(v);
        else if (strcmp(k, "threshold_mm") == 0)     b->threshold_mm = (uint16_t)atoi(v);
        else if (strcmp(k, "margin_mm") == 0)        b->margin_mm = (uint16_t)atoi(v);
        else if (strcmp(k, "worst_spread_mm") == 0)  b->worst_spread_mm = (uint16_t)atoi(v);
        else if (strcmp(k, "placement") == 0) {
            snprintf(b->placement, sizeof b->placement, "%s", v);
        }
        else if (strncmp(k, "zone", 4) == 0) {
            unsigned idx = (unsigned)atoi(k + 4);
            if (idx < P06_ZONES) {
                char *comma = strchr(v, ',');
                b->wall_mm[idx] = (uint16_t)atoi(v);
                if (comma != NULL)
                    b->spread_mm[idx] = (uint16_t)atoi(comma + 1);
            }
        }
    }

    if (fclose(fp) != 0)
        return P06_E_IO;
    if (!version_seen)
        return P06_E_FORMAT;
    if (b->threshold_mm == 0)
        return P06_E_FORMAT;
    return P06_OK;
}

p06_status p06_classify(const p06_frame *f, const p06_book *b, p06_occupancy *o)
{
    if (f == NULL || b == NULL || o == NULL)
        return P06_E_ARG;
    if (b->threshold_mm == 0)
        return P06_E_RANGE;

    memset(o, 0, sizeof *o);
    for (uint16_t i = 0; i < P06_ZONES; i++) {
        /* Zero is no target, and no target is not an occupied zone however
         * tempting the comparison looks. */
        bool occupied = (f->mm[i] > 0 && f->mm[i] < b->threshold_mm);
        o->occupied[i] = occupied;
        if (occupied)
            o->count++;
    }
    o->near_mm = p06_frame_near_mm(f, NULL);
    return P06_OK;
}
