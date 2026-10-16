/* The fix, the payload and the bearer.
 *
 * Three small readers, and each of them refuses rather than fills in. The
 * receiver's stamp is the only timestamp this lab will carry, so a line that
 * does not have one yields no fix at all and the cycle deals with that.
 */

#include "p03.h"

#include <stdlib.h>
#include <string.h>

const char *p03_strerror(p03_status s)
{
    switch (s) {
    case P03_OK:       return "ok";
    case P03_E_IO:     return "the file could not be read or written";
    case P03_E_FORMAT: return "the content is not in the expected form";
    case P03_E_ARG:    return "an argument is missing or will not do";
    case P03_E_AT:     return "the module refused the command, or said nothing";
    case P03_E_BEARER: return "the radio is not on Cat-M";
    case P03_E_ORDER:  return "a transmit was attempted with the receiver powered";
    }
    return "unknown";
}

void p03_fix_clear(p03_fix *f)
{
    if (f != NULL)
        memset(f, 0, sizeof *f);
}

bool p03_fix_parse(const char *line, p03_fix *f)
{
    if (line == NULL || f == NULL)
        return false;

    char field[P03_COORD_MAX];
    /* Field 1 is the fix status. Field 0 is whether the receiver is running,
     * which is true the moment it is powered and says nothing about whether
     * it has found anything. */
    if (!at_field(line, 1, field, sizeof field) || atoi(field) != 1)
        return false;

    char stamp[P03_STAMP_MAX];
    char lat[P03_COORD_MAX];
    char lon[P03_COORD_MAX];
    if (!at_field(line, 2, stamp, sizeof stamp))
        return false;
    if (!at_field(line, 3, lat, sizeof lat))
        return false;
    if (!at_field(line, 4, lon, sizeof lon))
        return false;

    p03_fix_clear(f);
    f->valid = true;
    f->cached = false;
    snprintf(f->stamp, sizeof f->stamp, "%s", stamp);
    snprintf(f->lat, sizeof f->lat, "%s", lat);
    snprintf(f->lon, sizeof f->lon, "%s", lon);
    return true;
}

size_t p03_payload(const p03_fix *f, char *out, size_t cap)
{
    if (f == NULL || out == NULL || cap == 0)
        return 0;
    out[0] = '\0';
    if (!f->valid)
        return 0;

    /* The stamp goes out exactly as the receiver gave it, cached or not. A
     * repeated stamp is how a receiver sees that a cycle produced nothing
     * new, and rewriting it with the host clock would hide that. */
    int n = snprintf(out, cap, "%s,%s,%s", f->stamp, f->lat, f->lon);
    if (n < 0 || (size_t)n >= cap)
        return 0;
    return (size_t)n;
}

bool p03_bearer_is_catm(const char *line)
{
    if (line == NULL)
        return false;
    /* Both spellings appear in the field: the marketing name and the 3GPP
     * one. Neither NB-IoT nor a GPRS fallback carries either. */
    return strstr(line, "CAT-M") != NULL ||
           strstr(line, "CAT_M") != NULL ||
           strstr(line, "eMTC") != NULL;
}
