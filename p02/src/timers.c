/* The two bitmaps, in seconds, and what they add up to with a send period.
 *
 * Both timers are three bits of unit and five bits of value, and the two
 * unit tables are different. They are in 3GPP TS 24.008: the extended
 * periodic update timer T3412 as GPRS Timer 3, section 10.5.7.4a, and the
 * active time T3324 as GPRS Timer, section 10.5.7.3.
 *
 * The tables below are read from that specification and nothing on this
 * bench has confirmed them against a network. A module's own manual may
 * accept a narrower set, and the value a network grants is a third thing
 * again, so the figures this file prints are the meaning of the request.
 */

#include "p02.h"

#include <string.h>

const char *p02_strerror(p02_status s)
{
    switch (s) {
    case P02_OK:       return "ok";
    case P02_E_IO:     return "the file could not be read or written";
    case P02_E_FORMAT: return "the content is not in the expected form";
    case P02_E_RANGE:  return "a value is outside the range this program allows";
    case P02_E_ARG:    return "an argument is missing or will not do";
    case P02_E_AT:     return "the module refused the command, or said nothing";
    case P02_E_PLAN:   return "the schedule and the timers contradict each other";
    }
    return "unknown";
}

/* GPRS Timer 3, 10.5.7.4a. The unit is the multiplier for the five-bit
 * value, and 111 means the timer is off. */
static uint32_t tau_unit_seconds(uint8_t unit, bool *deactivated)
{
    *deactivated = false;
    switch (unit) {
    case 0: return 10u * 60u;        /* 10 minutes */
    case 1: return 60u * 60u;        /* 1 hour */
    case 2: return 10u * 60u * 60u;  /* 10 hours */
    case 3: return 2u;               /* 2 seconds */
    case 4: return 30u;              /* 30 seconds */
    case 5: return 60u;              /* 1 minute */
    case 6: return 320u * 60u * 60u; /* 320 hours */
    default:
        *deactivated = true;
        return 0u;
    }
}

/* GPRS Timer, 10.5.7.3. Three units are defined, 111 means deactivated, and
 * the specification says the remaining values are to be read as one minute
 * rather than treated as an error. That clause is why this returns a number
 * for a unit it does not recognise instead of refusing. */
static uint32_t active_unit_seconds(uint8_t unit, bool *deactivated)
{
    *deactivated = false;
    switch (unit) {
    case 0: return 2u;               /* 2 seconds */
    case 1: return 60u;              /* 1 minute */
    case 2: return 6u * 60u;         /* a decihour, which is 6 minutes */
    case 7:
        *deactivated = true;
        return 0u;
    default:
        return 60u;                  /* the specification's catch-all */
    }
}

p02_status p02_timer_parse(p02_timer *t, p02_timer_kind kind, const char *bits)
{
    if (t == NULL || bits == NULL)
        return P02_E_ARG;
    if (strlen(bits) != P02_BITS)
        return P02_E_FORMAT;

    memset(t, 0, sizeof *t);
    uint8_t unit = 0;
    uint8_t value = 0;
    for (int i = 0; i < P02_BITS; i++) {
        if (bits[i] != '0' && bits[i] != '1')
            return P02_E_FORMAT;
        uint8_t bit = (uint8_t)(bits[i] == '1' ? 1 : 0);
        if (i < 3)
            unit = (uint8_t)((unit << 1) | bit);
        else
            value = (uint8_t)((value << 1) | bit);
    }
    memcpy(t->bits, bits, P02_BITS);
    t->bits[P02_BITS] = '\0';
    t->unit = unit;
    t->value = value;

    bool off = false;
    uint32_t mult = (kind == P02_TIMER_TAU)
                  ? tau_unit_seconds(unit, &off)
                  : active_unit_seconds(unit, &off);
    t->deactivated = off;
    t->seconds = off ? 0u : mult * value;
    return P02_OK;
}

static void human(uint32_t seconds, char *out, size_t cap)
{
    if (seconds == 0) {
        snprintf(out, cap, "0 s");
        return;
    }
    if (seconds % 3600u == 0u)
        snprintf(out, cap, "%lu h", (unsigned long)(seconds / 3600u));
    else if (seconds % 60u == 0u)
        snprintf(out, cap, "%lu min", (unsigned long)(seconds / 60u));
    else
        snprintf(out, cap, "%lu s", (unsigned long)seconds);
}

void p02_timer_print(const p02_timer *t, p02_timer_kind kind, FILE *out)
{
    if (t == NULL || out == NULL)
        return;
    const char *name = (kind == P02_TIMER_TAU)
                     ? "T3412 extended, the periodic update interval"
                     : "T3324, the active time after each exchange";
    if (t->deactivated) {
        fprintf(out, "%s: %s, unit %u value %u, deactivated\n",
                t->bits, name, (unsigned)t->unit, (unsigned)t->value);
        return;
    }
    char pretty[32];
    human(t->seconds, pretty, sizeof pretty);
    fprintf(out, "%s: %s, unit %u value %u, %lu s (%s)\n",
            t->bits, name, (unsigned)t->unit, (unsigned)t->value,
            (unsigned long)t->seconds, pretty);
}

p02_status p02_plan_make(p02_plan *p, const p02_timer *tau,
                         const p02_timer *active, uint32_t period_s)
{
    if (p == NULL || tau == NULL || active == NULL)
        return P02_E_ARG;
    if (period_s == 0)
        return P02_E_RANGE;

    memset(p, 0, sizeof *p);
    p->tau = *tau;
    p->active = *active;
    p->period_s = period_s;
    p->sends_per_hour = 3600.0 / (double)period_s;

    /* Each wake costs the active time, during which the radio is reachable
     * and drawing. Anything the module does outside that window is the
     * network's business and is not counted here, which is why this is the
     * floor of the duty cycle rather than a prediction of the current. */
    double awake_per_wake = active->deactivated ? 0.0 : (double)active->seconds;
    p->awake_seconds_per_hour = p->sends_per_hour * awake_per_wake;
    p->awake_fraction = p->awake_seconds_per_hour / 3600.0;

    /* The failure the chapter warns about, as a comparison. If the node
     * sends again before the active window has closed, the radio never
     * reaches power saving at all and the lab proves the opposite of its
     * intent. */
    p->radio_ever_sleeps = active->deactivated
                         ? true
                         : ((double)period_s > awake_per_wake);
    p->tau_outlives_period = (!tau->deactivated && tau->seconds > period_s);
    return P02_OK;
}

void p02_plan_print(const p02_plan *p, FILE *out)
{
    if (p == NULL || out == NULL)
        return;
    p02_timer_print(&p->tau, P02_TIMER_TAU, out);
    p02_timer_print(&p->active, P02_TIMER_ACTIVE, out);
    fprintf(out, "send period %lu s, which is %.2f datagrams an hour\n",
            (unsigned long)p->period_s, p->sends_per_hour);
    fprintf(out, "reachable about %.0f s in the hour, %.2f per cent of it\n",
            p->awake_seconds_per_hour, p->awake_fraction * 100.0);
    if (!p->radio_ever_sleeps)
        fprintf(out, "the send period is inside the active window, so the "
                     "radio never reaches power saving\n");
    if (!p->tau_outlives_period)
        fprintf(out, "the update interval is not longer than the send period, "
                     "so the sends are what keeps the module registered rather "
                     "than the timer\n");
}

p02_status p02_payload_check(const char *hex, uint32_t max_bytes, uint32_t *bytes)
{
    if (hex == NULL)
        return P02_E_ARG;
    size_t n = strlen(hex);
    if (n == 0 || (n % 2u) != 0u)
        return P02_E_FORMAT;
    for (size_t i = 0; i < n; i++) {
        char c = hex[i];
        bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                  (c >= 'A' && c <= 'F');
        if (!ok)
            return P02_E_FORMAT;
    }
    uint32_t count = (uint32_t)(n / 2u);
    if (bytes != NULL)
        *bytes = count;
    if (max_bytes > 0 && count > max_bytes)
        return P02_E_RANGE;
    return P02_OK;
}
