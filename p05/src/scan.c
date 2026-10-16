/* Reading the file the board produced, and saying what is in it.
 *
 * The chapter's instruction is to check the file before looking at the
 * picture, because a plot hides a hole and a row count does not. This is
 * that instruction, with the three things a row count alone still misses:
 * an index that repeats, an index that goes backwards, and a sample outside
 * the range the input is allowed to see.
 */

#include "p05.h"

#include <stdlib.h>
#include <string.h>

const char *p05_strerror(p05_status s)
{
    switch (s) {
    case P05_OK:       return "ok";
    case P05_E_IO:     return "the file could not be read or written";
    case P05_E_FORMAT: return "the content is not in the expected form";
    case P05_E_ARG:    return "an argument is missing or will not do";
    }
    return "unknown";
}

void p05_scan_init(p05_scan *s)
{
    if (s == NULL)
        return;
    memset(s, 0, sizeof *s);
    s->first_index = -1;
    s->last_index = -1;
    for (uint32_t c = 0; c < P05_CHANNELS_MAX; c++) {
        s->min[c] = 0.0;
        s->max[c] = 0.0;
    }
}

/* index,ch0,ch1[,...]. Returns the channel count, or -1 when the row is not
 * a row. Nothing is guessed: a field that does not parse whole makes the
 * row malformed rather than a row with a zero in it. */
static int parse_row(const char *line, int64_t *index, double *v, uint32_t cap)
{
    char *end = NULL;
    long long ix = strtoll(line, &end, 10);
    if (end == line)
        return -1;
    *index = (int64_t)ix;

    uint32_t n = 0;
    const char *p = end;
    while (*p == ',' && n < cap) {
        p++;
        char *vend = NULL;
        double x = strtod(p, &vend);
        if (vend == p)
            return -1;
        v[n++] = x;
        p = vend;
    }
    while (*p == ' ')
        p++;
    if (*p != '\0')
        return -1;
    return (int)n;
}

p05_status p05_scan_read(const char *path, p05_scan *s)
{
    if (path == NULL || s == NULL)
        return P05_E_ARG;
    p05_scan_init(s);

    linereader r;
    if (linereader_open(&r, path) != LINEREADER_OK)
        return P05_E_IO;

    double sum[P05_CHANNELS_MAX];
    memset(sum, 0, sizeof sum);
    bool first_row = true;
    int64_t previous = 0;

    for (;;) {
        const char *line = NULL;
        size_t len = 0;
        if (linereader_line(&r, &line, &len) != LINEREADER_OK)
            break;

        if (!s->header_seen && (line[0] < '0' || line[0] > '9')) {
            /* The header names the columns, and only the first such line is
             * a header. A second one means two files were concatenated. */
            s->header_seen = true;
            continue;
        }

        int64_t ix = 0;
        double v[P05_CHANNELS_MAX];
        int n = parse_row(line, &ix, v, P05_CHANNELS_MAX);
        if (n <= 0) {
            s->malformed++;
            continue;
        }
        if (s->channels == 0)
            s->channels = (uint32_t)n;
        else if ((uint32_t)n != s->channels) {
            /* A row with a different number of columns is not a row of this
             * file, whatever else it is. */
            s->malformed++;
            continue;
        }

        if (first_row) {
            s->first_index = ix;
            s->starts_at_zero = (ix == 0);
            for (uint32_t c = 0; c < s->channels; c++) {
                s->min[c] = v[c];
                s->max[c] = v[c];
            }
            first_row = false;
        } else {
            int64_t step = ix - previous;
            if (step == 0)
                s->repeats++;
            else if (step < 0)
                s->backwards++;
            else if (step > 1)
                s->holes += (uint32_t)(step - 1);
        }

        for (uint32_t c = 0; c < s->channels; c++) {
            sum[c] += v[c];
            if (v[c] < s->min[c])
                s->min[c] = v[c];
            if (v[c] > s->max[c])
                s->max[c] = v[c];
            if (v[c] > P05_INPUT_LIMIT_V || v[c] < -P05_INPUT_LIMIT_V)
                s->out_of_range++;
        }

        previous = ix;
        s->last_index = ix;
        s->rows++;
    }

    linereader_close(&r);

    for (uint32_t c = 0; c < s->channels; c++)
        s->mean[c] = (s->rows > 0) ? sum[c] / (double)s->rows : 0.0;
    return P05_OK;
}

bool p05_scan_whole(const p05_scan *s)
{
    if (s == NULL || s->rows == 0)
        return false;
    return s->holes == 0 && s->repeats == 0 && s->backwards == 0 &&
           s->malformed == 0 && s->starts_at_zero;
}

void p05_scan_print(const p05_scan *s, FILE *out)
{
    if (s == NULL || out == NULL)
        return;

    fprintf(out, "%lu rows, %lu channels, index %lld to %lld\n",
            (unsigned long)s->rows, (unsigned long)s->channels,
            (long long)s->first_index, (long long)s->last_index);
    fprintf(out, "holes %lu, repeats %lu, backwards %lu, malformed rows %lu\n",
            (unsigned long)s->holes, (unsigned long)s->repeats,
            (unsigned long)s->backwards, (unsigned long)s->malformed);
    for (uint32_t c = 0; c < s->channels; c++)
        fprintf(out, "ch%lu  mean %.4f V, min %.4f, max %.4f\n",
                (unsigned long)c, s->mean[c], s->min[c], s->max[c]);
    if (s->out_of_range > 0)
        fprintf(out, "%lu samples beyond the %.1f V input limit, which is a "
                     "wiring question rather than a file question\n",
                (unsigned long)s->out_of_range, P05_INPUT_LIMIT_V);
    if (!s->starts_at_zero && s->rows > 0)
        fprintf(out, "the index starts at %lld rather than zero\n",
                (long long)s->first_index);
}
