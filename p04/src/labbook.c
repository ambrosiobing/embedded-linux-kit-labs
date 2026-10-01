/* The lab book: a baseline and the conditions it was taken under.
 *
 * A flat key=value file rather than JSON. The program then depends on nothing
 * outside the C library, the file can be read with cat and edited with vi on a
 * board that has neither Python nor a JSON tool, and a corrupted line is a
 * corrupted line rather than a parse that fails at byte zero.
 *
 * The mounting is free text and it is not decoration. The same machine on a
 * magnet and on a stud gives different numbers, so a baseline without a record
 * of how the probe was held is a number nobody can reproduce.
 */

#include "p04.h"

#include <stdlib.h>
#include <string.h>

p04_status p04_labbook_write(const char *path, const p04_thresholds *t)
{
    if (path == NULL || t == NULL)
        return P04_E_ARG;
    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return P04_E_IO;

    int n = fprintf(fp,
        "# P04 lab book. Every number below was measured under the mounting\n"
        "# named at the end, and means nothing under a different one.\n"
        "version=1\n"
        "baseline=%.9g\n"
        "warn_ratio=%.9g\n"
        "fault_ratio=%.9g\n"
        "band_low_hz=%.9g\n"
        "band_high_hz=%.9g\n"
        "rate_hz=%.9g\n"
        "g_per_count=%.9g\n"
        "mounting=%s\n",
        (double)t->baseline, (double)t->warn_ratio, (double)t->fault_ratio,
        (double)t->band_low_hz, (double)t->band_high_hz,
        (double)t->rate_hz, (double)t->g_per_count, t->mounting);

    if (fclose(fp) != 0 || n < 0)
        return P04_E_IO;
    return P04_OK;
}

static void trim_newline(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

p04_status p04_labbook_read(const char *path, p04_thresholds *t)
{
    if (path == NULL || t == NULL)
        return P04_E_ARG;
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return P04_E_IO;

    p04_thresholds_default(t);
    bool saw_version = false;
    char line[256];

    while (fgets(line, (int)sizeof line, fp) != NULL) {
        trim_newline(line);
        if (line[0] == '#' || line[0] == '\0')
            continue;
        char *eq = strchr(line, '=');
        if (eq == NULL)
            continue;           /* a line without a key is skipped, not fatal */
        *eq = '\0';
        const char *key = line;
        const char *val = eq + 1;

        if (strcmp(key, "version") == 0) {
            saw_version = true;
            if (atoi(val) != 1) {
                fclose(fp);
                return P04_E_FORMAT;
            }
        } else if (strcmp(key, "baseline") == 0) {
            t->baseline = (float)atof(val);
        } else if (strcmp(key, "warn_ratio") == 0) {
            t->warn_ratio = (float)atof(val);
        } else if (strcmp(key, "fault_ratio") == 0) {
            t->fault_ratio = (float)atof(val);
        } else if (strcmp(key, "band_low_hz") == 0) {
            t->band_low_hz = (float)atof(val);
        } else if (strcmp(key, "band_high_hz") == 0) {
            t->band_high_hz = (float)atof(val);
        } else if (strcmp(key, "rate_hz") == 0) {
            t->rate_hz = (float)atof(val);
        } else if (strcmp(key, "g_per_count") == 0) {
            t->g_per_count = (float)atof(val);
        } else if (strcmp(key, "mounting") == 0) {
            snprintf(t->mounting, sizeof t->mounting, "%s", val);
        }
    }
    fclose(fp);

    if (!saw_version)
        return P04_E_FORMAT;
    if (!(t->band_high_hz > t->band_low_hz) || t->rate_hz <= 0.0f)
        return P04_E_FORMAT;
    if (t->fault_ratio < t->warn_ratio)
        return P04_E_FORMAT;
    return P04_OK;
}
