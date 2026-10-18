/* Captures the bench did not take.
 *
 * Five kinds, and they are not all the same sort of thing. `ramp` exists so
 * the suite can check the quantiles against arithmetic: its samples are
 * 1, 2, 3 microamps and so on, so the median and the p95 of n of them are
 * worked out on paper rather than observed from the program being tested.
 * The other four exist so the judge can be exercised, and the suite asserts
 * relations between them rather than absolute figures, because a plausible
 * number from a generator is not a measurement of anything.
 *
 * The three state kinds are shaped the way an ESP32 is shaped and not the
 * way a tidy test would be. In particular `ble` does not sit at the same
 * floor as `idle`: advertising keeps the radio subsystem powered, so its
 * baseline is above radio-off idle and its bursts are above that again. A
 * generator where BLE and idle shared a baseline would fail the separation
 * test for a real reason, and that reason would be about the firmware rather
 * than about the cable, which is not what this lab's refusal means.
 *
 * `usb` is the failure the whole lab exists to catch: the small remainder
 * that still comes through the instrument when the DUT is also drawing from
 * its own USB cable. Written three times under three state names it produces
 * three traces that overlay, which is exactly what step 8 refuses.
 *
 * Deterministic throughout. The same kind and count give the same file every
 * time, because a suite that cannot reproduce its own input is not testing
 * the thing it names.
 */
#include "p10.h"

#include <string.h>

/* A plain linear congruential generator, written out rather than taken from
 * the library, because rand() is not required to give the same sequence on
 * two machines and a scene that differs between the laptop and the runner is
 * a scene that cannot be argued about. */
static uint32_t lcg(uint32_t *state)
{
    *state = *state * 1103515245u + 12345u;
    return (*state >> 16) & 0x7FFFu;
}

static uint32_t band(uint32_t *state, uint32_t lo_ua, uint32_t hi_ua)
{
    return lo_ua + lcg(state) % (hi_ua - lo_ua + 1u);
}

bool p10_scene_of(const char *name, p10_scene_kind *out)
{
    if (name == NULL || out == NULL)
        return false;
    if (strcmp(name, "ramp") == 0)
        *out = P10_SCENE_RAMP;
    else if (strcmp(name, "sleep") == 0)
        *out = P10_SCENE_SLEEP;
    else if (strcmp(name, "idle") == 0)
        *out = P10_SCENE_IDLE;
    else if (strcmp(name, "ble") == 0)
        *out = P10_SCENE_BLE;
    else if (strcmp(name, "usb") == 0)
        *out = P10_SCENE_USB;
    else
        return false;
    return true;
}

p10_status p10_scene_write(p10_scene_kind kind, uint32_t n, FILE *out)
{
    if (out == NULL || n == 0u)
        return P10_E_ARG;

    fprintf(out, "# a capture that did not happen: kind %d, %lu samples\n",
            (int)kind, (unsigned long)n);
    fprintf(out, "# microamps, one per line, which is the column the Power\n");
    fprintf(out, "# Profiler's own export writes\n");

    uint32_t s = 2463534242u + (uint32_t)kind * 7919u;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t ua;
        switch (kind) {
        case P10_SCENE_RAMP:
            /* 1, 2, 3, ... so that the median of n is ceil(n/2) exactly. */
            ua = i + 1u;
            break;
        case P10_SCENE_SLEEP:
            ua = band(&s, 40u, 50u);
            break;
        case P10_SCENE_IDLE:
            ua = band(&s, 20000u, 24000u);
            break;
        case P10_SCENE_BLE:
            /* One sample in ten is an advertising burst, so the p95 lands
             * inside the burst population rather than on its edge. */
            ua = (i % 10u == 0u) ? band(&s, 105000u, 115000u)
                                 : band(&s, 36000u, 40000u);
            break;
        case P10_SCENE_USB:
            ua = band(&s, 2000u, 3000u);
            break;
        default:
            return P10_E_ARG;
        }
        fprintf(out, "%lu\n", (unsigned long)ua);
    }
    return P10_OK;
}
