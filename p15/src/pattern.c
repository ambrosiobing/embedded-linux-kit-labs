/* The wiring table, the polarity, and the measured draw.
 *
 * All of it is arithmetic on two dated observations, and none of it needs a
 * chip. The two dates are in p15.h beside the tables they belong to, because
 * a wiring table and a current table are observations and not constants: the
 * first is true of one board on one afternoon, and the second is true of
 * four modules that happened to be the ones in this bin.
 */
#include "p15.h"

/* The fixed-point formatting moved to lib/thousandths.h when P10 turned out
 * to reduce a capture's quantiles with the same arithmetic. */
#include "../../lib/thousandths.h"

#include <stdio.h>
#include <string.h>

/* ----------------------------------------------------------------- wiring */

static const p15_wire WIRES[P15_CHANNELS] = {
    { 17u, 11u, "green",  "registered" },
    { 27u, 13u, "yellow", "a default route" },
    { 22u, 15u, "red",    "an event" }
};

const p15_wire *p15_wiring(void)
{
    return WIRES;
}

const p15_wire *p15_wire_of(unsigned channel)
{
    if (channel >= P15_CHANNELS)
        return NULL;
    return &WIRES[channel];
}

bool p15_chip_matches(const char *label)
{
    if (label == NULL)
        return false;
    return strcmp(label, P15_CHIP_LABEL) == 0;
}

/* --------------------------------------------------------------- polarity */

uint8_t p15_bits_for(uint8_t lit, p15_polarity polarity)
{
    uint8_t in_range = (uint8_t)(lit & P15_LIT_ALL);
    if (polarity == P15_SINKING)
        return (uint8_t)(~(unsigned)in_range & P15_LIT_ALL);
    return in_range;
}

/* The inversion is its own involution, so this is the same arithmetic. It is
 * written out rather than aliased so that the suite's round trip is a
 * statement about two functions instead of one function agreeing with
 * itself. */
uint8_t p15_lit_from(uint8_t bits, p15_polarity polarity)
{
    uint8_t in_range = (uint8_t)(bits & P15_LIT_ALL);
    if (polarity == P15_SINKING)
        return (uint8_t)(~(unsigned)in_range & P15_LIT_ALL);
    return in_range;
}

uint8_t p15_dark_bits(p15_polarity polarity)
{
    return p15_bits_for(P15_LIT_NONE, polarity);
}

/* ----------------------------------------------------------------- budget */

static const p15_module MODULES[P15_MODULES] = {
    { "blue",   9810u, 2940u },
    { "green", 15980u, 7390u },
    { "yellow", 13100u, 6200u },
    { "red",    13160u, 6340u }
};

const p15_module *p15_modules(void)
{
    return MODULES;
}

const p15_module *p15_module_of(const char *colour)
{
    if (colour == NULL)
        return NULL;
    for (unsigned i = 0; i < P15_MODULES; i++) {
        if (strcmp(MODULES[i].colour, colour) == 0)
            return &MODULES[i];
    }
    return NULL;
}

uint32_t p15_draw_ua(uint8_t lit)
{
    uint32_t total = 0;
    for (unsigned ch = 0; ch < P15_CHANNELS; ch++) {
        if ((lit & P15_LIT(ch)) == 0)
            continue;
        const p15_module *m = p15_module_of(WIRES[ch].colour);
        if (m != NULL)
            total += m->ua_at_3v3;
    }
    return total;
}

uint32_t p15_peak_channel_ua(uint8_t lit)
{
    uint32_t peak = 0;
    for (unsigned ch = 0; ch < P15_CHANNELS; ch++) {
        if ((lit & P15_LIT(ch)) == 0)
            continue;
        const p15_module *m = p15_module_of(WIRES[ch].colour);
        if (m != NULL && m->ua_at_3v3 > peak)
            peak = m->ua_at_3v3;
    }
    return peak;
}

/* lib/thousandths.h now, with P10. It stays as a function here so that this
 * lab's header still names the units it works in, and so that the suite's
 * carry check keeps testing the lifted arithmetic from the lab that found
 * the case. */
bool p15_format_ma(char *buf, size_t n, uint32_t ua)
{
    return thousandths_format(buf, n, ua, "mA");
}
