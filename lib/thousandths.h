/* A whole number of thousandths, printed as a unit with one decimal.
 *
 * 19930 microamps is 19.9 mA. 500102 nanoamps is 500.1 microamps. Both are
 * the same arithmetic on different units, which is why this is here rather
 * than in whichever lab needed it first: P15 reduces a measured current
 * table to milliamps, and P10 reduces a capture's quantiles to microamps and
 * milliamps both.
 *
 * Integer throughout. No lab in this volume that carries a measured current
 * has any floating point in it, so there is no rounding to argue about and
 * the strict build has nothing to say about promotions.
 *
 * It is a named function rather than two divisions at the call site because
 * the carry is a real case. A remainder that rounds up to ten has to carry
 * into the whole part, and a formatter that cannot do that prints "19.10"
 * on the day a measurement lands there. P15's suite asserts that case
 * directly, which is why it is written out here rather than assumed.
 *
 * Header only, with a static inline function, so no Makefile has to learn
 * about it.
 */
#ifndef THOUSANDTHS_H
#define THOUSANDTHS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Writes "<whole>.<tenth> <unit>" into buf. Returns false when buf is too
 * small, rather than writing half a number, so a caller that ignores the
 * answer still never prints a truncated figure. Needs the unit's length
 * plus seven bytes at worst for a 32-bit value. */
static inline bool thousandths_format(char *buf, size_t n, uint32_t value,
                                      const char *unit)
{
    if (buf == NULL || n == 0 || unit == NULL)
        return false;

    uint32_t whole = value / 1000u;
    uint32_t tenth = (value % 1000u + 50u) / 100u;
    if (tenth >= 10u) {          /* the carry, which is why this is a function */
        whole += 1u;
        tenth = 0u;
    }

    int w = snprintf(buf, n, "%lu.%lu %s",
                     (unsigned long)whole, (unsigned long)tenth, unit);
    return w > 0 && (size_t)w < n;
}

#endif /* THOUSANDTHS_H */
