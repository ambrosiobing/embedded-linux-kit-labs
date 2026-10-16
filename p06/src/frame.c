/* The contract between the two hosts, parsed strictly.
 *
 *   Z,d00,d01,...,d63
 *
 * Sixty-five comma-separated fields, the first the literal Z. That is the
 * whole format, it is the chapter's own words, and this file accepts exactly
 * it. Every other line is dropped with a reason, because the four ways a line
 * can be wrong have four different causes and a single counter would hide
 * which one the bench is suffering from.
 */

#include "p06.h"

#include <stdlib.h>
#include <string.h>

const char *p06_strerror(p06_status s)
{
    switch (s) {
    case P06_OK:       return "ok";
    case P06_E_IO:     return "the file could not be read or written";
    case P06_E_FORMAT: return "the content is not what this program expects";
    case P06_E_RANGE:  return "a value is outside the range this program allows";
    case P06_E_ARG:    return "an argument is missing or will not do";
    case P06_E_EOF:    return "the end of the stream";
    }
    return "unknown";
}

p06_status p06_parse_frame(const char *line, p06_frame *f, p06_drop *why)
{
    if (line == NULL || f == NULL)
        return P06_E_ARG;
    if (why != NULL)
        *why = P06_DROP_NONE;

    if (line[0] != 'Z' || line[1] != ',') {
        if (why != NULL)
            *why = P06_DROP_NOT_Z;
        return P06_E_FORMAT;
    }

    const char *p = line + 2;
    uint16_t n = 0;

    while (n < P06_ZONES) {
        if (*p == '\0') {
            /* The line ended before the contract did. */
            if (why != NULL)
                *why = P06_DROP_FIELD_COUNT;
            return P06_E_FORMAT;
        }
        if (*p < '0' || *p > '9') {
            /* No sign, no space, no empty field. A minus here would be read
             * by strtoul as a very large unsigned number, which is exactly
             * the sort of quiet repair this lab refuses. */
            if (why != NULL)
                *why = P06_DROP_NOT_INTEGER;
            return P06_E_FORMAT;
        }
        char *end = NULL;
        unsigned long v = strtoul(p, &end, 10);
        if (end == p) {
            if (why != NULL)
                *why = P06_DROP_NOT_INTEGER;
            return P06_E_FORMAT;
        }
        if (v > P06_RANGE_MAX_MM) {
            if (why != NULL)
                *why = P06_DROP_OUT_OF_RANGE;
            return P06_E_RANGE;
        }
        f->mm[n++] = (uint16_t)v;
        p = end;

        if (n < P06_ZONES) {
            if (*p != ',') {
                /* Fewer fields than the contract, or a separator that is not
                 * one. Either way the frame is short, and a short frame is
                 * the one the chapter says never to paint. */
                if (why != NULL)
                    *why = (*p == '\0') ? P06_DROP_FIELD_COUNT : P06_DROP_NOT_INTEGER;
                return P06_E_FORMAT;
            }
            p++;
        }
    }

    if (*p != '\0') {
        /* More fields than the contract. A sixty-fifth distance means the
         * firmware is not the one this chapter describes. */
        if (why != NULL)
            *why = P06_DROP_FIELD_COUNT;
        return P06_E_FORMAT;
    }
    return P06_OK;
}

uint16_t p06_frame_near_mm(const p06_frame *f, uint16_t *zones_with_target)
{
    uint16_t near = 0;
    uint16_t seen = 0;
    if (f == NULL) {
        if (zones_with_target != NULL)
            *zones_with_target = 0;
        return 0;
    }
    for (uint16_t i = 0; i < P06_ZONES; i++) {
        /* Zero is the sensor reporting no target in that zone. Treating it as
         * a distance would make an empty room read as something touching the
         * glass, which is the wrong answer in the most alarming direction. */
        if (f->mm[i] == 0)
            continue;
        seen++;
        if (near == 0 || f->mm[i] < near)
            near = f->mm[i];
    }
    if (zones_with_target != NULL)
        *zones_with_target = seen;
    return near;
}

uint16_t p06_frame_far_mm(const p06_frame *f)
{
    uint16_t far = 0;
    if (f == NULL)
        return 0;
    for (uint16_t i = 0; i < P06_ZONES; i++)
        if (f->mm[i] > far)
            far = f->mm[i];
    return far;
}

void p06_frame_print(const p06_frame *f, const p06_book *b, FILE *out)
{
    if (f == NULL || out == NULL)
        return;
    for (uint16_t row = 0; row < P06_SIDE; row++) {
        for (uint16_t col = 0; col < P06_SIDE; col++) {
            uint16_t mm = f->mm[row * P06_SIDE + col];
            char mark = ' ';
            if (mm == 0)
                mark = '.';           /* no target */
            else if (b != NULL && b->threshold_mm > 0 && mm < b->threshold_mm)
                mark = '*';           /* occupied */
            fprintf(out, "%5u%c", (unsigned)mm, mark);
        }
        fprintf(out, "\n");
    }
}
