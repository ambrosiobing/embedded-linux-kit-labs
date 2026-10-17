/* The ADXL345, for the labs that read one.
 *
 * Three of them do. P01 publishes shock events from a Pi, P09 runs that same
 * publisher on a NanoPi, and P19 turns the same six bytes into two angles on
 * a panel. The registers and the decode are identical in all three, because
 * they are properties of the part rather than of any lab.
 *
 * The decode is eight lines and half of it is the part that is usually
 * wrong. Two bytes arrive low first, they are two's complement, and a
 * reading of minus one gravity is 0xFF00 rather than 65280 counts of
 * acceleration. A sign extension missed here produces a magnitude that is
 * enormous and an event on every sample, which looks like a broken sensor
 * and sends a bench chasing its wiring.
 *
 * Header only, with static inline functions, like the other small files in
 * here. There is nothing to build and nothing for three programs to collide
 * over.
 */
#ifndef ADXL345_H
#define ADXL345_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define ADXL345_ADDRESS 0x53
#define ADXL345_REG_POWER_CTL 0x2D
#define ADXL345_REG_DATA_FORMAT 0x31
#define ADXL345_REG_DATAX0 0x32
#define ADXL345_POWER_MEASURE 0x08
#define ADXL345_FORMAT_FULL_RES_16G 0x0B
/* Full resolution is 3.9 milli-g per count, which is one 256th of a gravity
 * at every range. That is the whole reason full resolution is selected: the
 * scale stops depending on the range, so a threshold written in gravities
 * means the same thing whatever the range is set to. */
#define ADXL345_COUNTS_PER_G 256.0

typedef struct {
    double ax, ay, az;
    double magnitude;
} adxl345_sample;

static inline void adxl345_decode(const uint8_t raw[6], adxl345_sample *out)
{
    if (raw == NULL || out == NULL)
        return;

    double g[3];
    for (int axis = 0; axis < 3; axis++) {
        uint16_t u = (uint16_t)((uint16_t)raw[axis * 2 + 1] << 8 |
                                (uint16_t)raw[axis * 2]);
        /* Through int32_t rather than by subtracting 65536 from a double, so
         * that the two's complement is done in integers where it is exact. */
        int32_t v = (int32_t)u;
        if (u & 0x8000u)
            v -= 65536;
        g[axis] = (double)v / ADXL345_COUNTS_PER_G;
    }
    out->ax = g[0];
    out->ay = g[1];
    out->az = g[2];
    out->magnitude = sqrt(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
}

/* "f0 ff 10 00 00 01", six byte values in hexadecimal, separated by spaces
 * or commas, and nothing else on the line. Hexadecimal and not "whatever
 * parses", because that is what the device returns, what a bench capture
 * records and what these programs write: one base, so that no line can mean
 * two things. A leading 0x is accepted. */
static inline bool adxl345_parse_bytes(const char *text, uint8_t raw[6])
{
    if (text == NULL || raw == NULL)
        return false;

    const char *p = text;
    for (int i = 0; i < 6; i++) {
        while (*p == ' ' || *p == ',' || *p == '\t')
            p++;
        if (*p == '\0')
            return false;
        char *end = NULL;
        long v = strtol(p, &end, 16);
        if (end == p || v < 0 || v > 255)
            return false;
        raw[i] = (uint8_t)v;
        p = end;
    }
    while (*p == ' ' || *p == ',' || *p == '\t')
        p++;
    return *p == '\0';
}

#endif /* ADXL345_H */
