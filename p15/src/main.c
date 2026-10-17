/* P15's semaphore, on the command line.
 *
 * Three of the six subcommands need no chip and no board: `plan`, `budget`
 * and `wiring` print what the program would do and what it was told, and
 * they are the ones a reader can run on the laptop that is writing the lab
 * book. The other three need the bench.
 *
 * `plan` exists so that a sitting can be prepared and checked on paper. The
 * table it prints is produced by the same p15_frame_at() that drives the
 * pins in `blink`, so what the suite proves about the plan is a statement
 * about what the LEDs will do, rather than about a second description of it.
 */
#define _POSIX_C_SOURCE 200809L

#include "p15.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define EXIT_USAGE  1
#define EXIT_DEVICE 2

static const char *USAGE =
    "usage: p15 <command> [options]\n"
    "\n"
    "  no bench needed\n"
    "    plan                 the blink, step by step, with the draw of each\n"
    "    budget               the measured modules, and what a lit set draws\n"
    "    wiring               the pins, the offsets and the chip they are from\n"
    "\n"
    "  needs the chip\n"
    "    probe                what holds the three lines, before asking for them\n"
    "    semaphore --state S  hold one state; S is registered, route, event,\n"
    "                         all or dark\n"
    "    blink                run the plan\n"
    "\n"
    "  options\n"
    "    --seconds N          how long `semaphore` holds (default 10)\n"
    "    --cycles N           how many plan cycles `blink` runs (default 3)\n"
    "    --chip PATH          default " P15_CHIP_PATH "\n"
    "    --sinking            the modules sink, so low is lit; this bench's\n"
    "                         do not, and the chapter says to settle that by\n"
    "                         hand before trusting any program\n"
    "    --any-chip           drive the offsets even though the chip's label\n"
    "                         is not the one the wiring was recorded against\n";

/* ------------------------------------------------------------- printing */

static void lit_names(uint8_t lit, char *buf, size_t n)
{
    if (n == 0)
        return;
    buf[0] = '\0';

    size_t used = 0;
    for (unsigned ch = 0; ch < P15_CHANNELS; ch++) {
        if ((lit & P15_LIT(ch)) == 0)
            continue;
        const p15_wire *w = p15_wire_of(ch);
        int k = snprintf(buf + used, n - used, "%s%s",
                         (used > 0) ? " " : "", w->colour);
        if (k < 0 || (size_t)k >= n - used)
            return;
        used += (size_t)k;
    }
    if (used == 0)
        snprintf(buf, n, "dark");
}

static void print_bits(uint8_t lit, p15_polarity pol)
{
    uint8_t bits = p15_bits_for(lit, pol);
    for (unsigned ch = 0; ch < P15_CHANNELS; ch++)
        putchar(((bits & P15_LIT(ch)) != 0) ? '1' : '0');
}

/* ------------------------------------------------------- no bench needed */

static int cmd_plan(p15_polarity pol)
{
    printf("The blink, %u steps of %u ms, %u ms to a cycle.\n",
           P15_STEPS, P15_STEP_MS, P15_PLAN_MS);
    printf("Line values are in channel order, green yellow red, under the "
           "%s arrangement.\n\n",
           (pol == P15_SOURCING) ? "sourcing" : "sinking");

    printf("  step   from     to   lines   draw      lit\n");
    for (unsigned i = 0; i < P15_STEPS; i++) {
        uint8_t lit = p15_step_lit(i);
        char names[48];
        char ma[16];
        lit_names(lit, names, sizeof names);
        if (!p15_format_ma(ma, sizeof ma, p15_draw_ua(lit)))
            return EXIT_DEVICE;

        printf("  %4u %6u %6u   ", i, i * P15_STEP_MS, (i + 1u) * P15_STEP_MS);
        print_bits(lit, pol);
        printf("   %-9s %s\n", ma, names);
    }

    printf("\nThe fourth step is the acceptance item: three modules lit at "
           "once, in one\ncall rather than three. The three before it are "
           "what make it a statement\nabout three channels instead of three "
           "LEDs, because a reader who has crossed\ntwo jumpers sees the "
           "fourth step pass.\n");
    printf("\nThe cycle ends dark, so a program that is still running can be "
           "told from one\nthat has exited: the first goes dark and comes "
           "back.\n");
    return 0;
}

static int cmd_budget(void)
{
    printf("Measured Saturday 3 October 2026, each module alone on a source "
           "meter.\n\n");

    const p15_module *mods = p15_modules();
    printf("  module    at 5.000 V   at 3.300 V\n");
    for (unsigned i = 0; i < P15_MODULES; i++) {
        char a[16];
        char b[16];
        if (!p15_format_ma(a, sizeof a, mods[i].ua_at_5v0) ||
            !p15_format_ma(b, sizeof b, mods[i].ua_at_3v3))
            return EXIT_DEVICE;
        printf("  %-9s %10s   %10s\n", mods[i].colour, a, b);
    }

    printf("\nThis semaphore is green, yellow and red, which are the colours "
           "P01, P02 and\nP03 give meanings to. At 3.3 V:\n\n");

    uint8_t sets[4] = { P15_LIT_NONE, P15_LIT(P15_GREEN), P15_LIT_ALL,
                        (uint8_t)(P15_LIT(P15_GREEN) | P15_LIT(P15_RED)) };
    for (unsigned i = 0; i < 4u; i++) {
        char names[48];
        char sum[16];
        char peak[16];
        lit_names(sets[i], names, sizeof names);
        if (!p15_format_ma(sum, sizeof sum, p15_draw_ua(sets[i])) ||
            !p15_format_ma(peak, sizeof peak, p15_peak_channel_ua(sets[i])))
            return EXIT_DEVICE;
        printf("  %-20s sum %-9s largest single pin %s\n",
               names, sum, peak);
    }

    /* The chapter's figure, and the triple it came from. Both numbers are
     * right about their own three modules. */
    const p15_module *blue = p15_module_of("blue");
    const p15_module *green = p15_module_of("green");
    const p15_module *yellow = p15_module_of("yellow");
    if (blue == NULL || green == NULL || yellow == NULL)
        return EXIT_DEVICE;

    char chapter[16];
    char here[16];
    if (!p15_format_ma(chapter, sizeof chapter,
                       blue->ua_at_3v3 + green->ua_at_3v3 + yellow->ua_at_3v3) ||
        !p15_format_ma(here, sizeof here, p15_draw_ua(P15_LIT_ALL)))
        return EXIT_DEVICE;

    printf("\nThe chapter quotes %s for \"the semaphore\" and names the three "
           "modules it\nmeasured: blue, green and yellow. This semaphore's "
           "three are green, yellow\nand red, and they draw %s. Both figures "
           "are right about their own triple.\nThe sum is the number a "
           "whole-header limit is compared against, and the largest\nsingle "
           "pin is the number a per-pin limit is compared against. This "
           "volume\nprints no figure for either, so nothing here is set "
           "against a limit.\n", chapter, here);
    return 0;
}

static int cmd_wiring(void)
{
    printf("Recorded Friday 2 October 2026 on a Raspberry Pi 3 Model B "
           "Rev 1.2.\n\n");
    printf("  channel  colour   header pin   line offset   means\n");
    const p15_wire *w = p15_wiring();
    for (unsigned ch = 0; ch < P15_CHANNELS; ch++) {
        printf("  %7u  %-7s %12u %13u   %s\n",
               ch, w[ch].colour, w[ch].header_pin, w[ch].offset,
               w[ch].meaning);
    }
    printf("\nThe offsets are the BCM numbers because on %s they are the "
           "same number.\nThat is a property of this chip and not a general "
           "truth, so nothing is\ndriven before the chip has given its label."
           " Offsets 17, 27 and 22 on a chip\nthat numbers its lines "
           "differently are three arbitrary pins.\n", P15_CHIP_LABEL);
    printf("\nHigh is lit on this bench, settled by the chapter's three "
           "controls in the\norder high, low, released: lit, dark, dark.\n");
    return 0;
}

/* ------------------------------------------------------- needs the chip */

static void print_report(const p15_report *r)
{
    printf("chip %s, %u lines\n", r->chip_label, r->chip_lines);
    if (!p15_chip_matches(r->chip_label)) {
        printf("  and that is not %s, which is the chip this lab's wiring "
               "table is from\n", P15_CHIP_LABEL);
    }
    const p15_wire *w = p15_wiring();
    for (unsigned ch = 0; ch < P15_CHANNELS; ch++) {
        printf("  offset %2u  pin %2u  %-7s  %s", w[ch].offset,
               w[ch].header_pin, w[ch].colour,
               r->used[ch] ? "held" : "free");
        if (r->used[ch] && r->consumer[ch][0] != '\0')
            printf(" by %s", r->consumer[ch]);
        putchar('\n');
    }
}

static int cmd_probe(const char *chip)
{
    p15_report r;
    p15_status s = p15_probe(chip, &r);
    if (s != P15_OK) {
        fprintf(stderr, "p15: probe: %s\n", p15_strstatus(s));
        return EXIT_DEVICE;
    }
    print_report(&r);
    printf("\nA held line is reported rather than refused here, because what "
           "is on the chip\nis the question. A request for a held line is "
           "what fails, and it fails with\nthe holder's name.\n");
    return 0;
}

/* A refusal that names the holder rather than an errno. The probe is a
 * second call and could in principle disagree with the request, which is
 * worth saying: between the two, something else may have let the line go. */
static int report_open_failure(p15_status s, const char *chip)
{
    fprintf(stderr, "p15: %s\n", p15_strstatus(s));
    if (s == P15_E_BUSY || s == P15_E_WRONG_CHIP) {
        p15_report r;
        if (p15_probe(chip, &r) == P15_OK) {
            fprintf(stderr, "\n");
            print_report(&r);
        }
    }
    return EXIT_DEVICE;
}

static bool hold_ms(uint32_t ms)
{
    struct timespec t;
    t.tv_sec = (time_t)(ms / 1000u);
    t.tv_nsec = (long)(ms % 1000u) * 1000000L;
    return nanosleep(&t, NULL) == 0;
}

static int cmd_semaphore(uint8_t lit, uint32_t seconds, const char *chip,
                         bool any_chip, p15_polarity pol)
{
    p15_lines l;
    p15_status s = p15_lines_open(&l, chip, any_chip, pol);
    if (s != P15_OK)
        return report_open_failure(s, chip);

    s = p15_lines_set(&l, lit);
    if (s != P15_OK) {
        fprintf(stderr, "p15: %s\n", p15_strstatus(s));
        p15_lines_close(&l);
        return EXIT_DEVICE;
    }

    char names[48];
    lit_names(lit, names, sizeof names);
    char ma[16];
    if (!p15_format_ma(ma, sizeof ma, p15_draw_ua(lit))) {
        p15_lines_close(&l);
        return EXIT_DEVICE;
    }
    printf("holding %s for %lu s, lines ", names, (unsigned long)seconds);
    print_bits(lit, pol);
    printf(", %s\n", ma);
    printf("The lines are released when this process ends, however it ends. "
           "There is no\nhandler here to do it: the kernel closes the "
           "descriptor and the lines go with\nit, which is a guarantee with "
           "no code path through it.\n");

    (void)hold_ms(seconds * 1000u);
    p15_lines_close(&l);
    printf("released\n");
    return 0;
}

static int cmd_blink(uint32_t cycles, const char *chip, bool any_chip,
                     p15_polarity pol)
{
    p15_lines l;
    p15_status s = p15_lines_open(&l, chip, any_chip, pol);
    if (s != P15_OK)
        return report_open_failure(s, chip);

    printf("%lu cycles of %u ms. The fourth step of each is the acceptance "
           "item.\n", (unsigned long)cycles, P15_PLAN_MS);

    /* Driven from p15_frame_at() rather than from a loop over the steps, so
     * that the sequence the suite checks is the sequence the pins see. */
    uint32_t total = cycles * P15_PLAN_MS;
    for (uint32_t t = 0; t < total; t += P15_STEP_MS) {
        uint8_t lit = p15_frame_at(t);
        s = p15_lines_set(&l, lit);
        if (s != P15_OK) {
            fprintf(stderr, "p15: at %lu ms: %s\n",
                    (unsigned long)t, p15_strstatus(s));
            p15_lines_close(&l);
            return EXIT_DEVICE;
        }

        char names[48];
        lit_names(lit, names, sizeof names);
        printf("  %6lu ms  ", (unsigned long)t);
        print_bits(lit, pol);
        printf("  %s\n", names);
        fflush(stdout);

        if (!hold_ms(P15_STEP_MS)) {
            /* Interrupted. The lines release on the way out, so this is a
             * short run and not a semaphore left lit. */
            break;
        }
    }

    p15_lines_close(&l);
    printf("released, and the semaphore is dark\n");
    return 0;
}

/* ------------------------------------------------------------------ args */

static bool state_of(const char *name, uint8_t *lit)
{
    if (name == NULL || lit == NULL)
        return false;
    if (strcmp(name, "registered") == 0 || strcmp(name, "green") == 0)
        *lit = P15_LIT(P15_GREEN);
    else if (strcmp(name, "route") == 0 || strcmp(name, "yellow") == 0)
        *lit = P15_LIT(P15_YELLOW);
    else if (strcmp(name, "event") == 0 || strcmp(name, "red") == 0)
        *lit = P15_LIT(P15_RED);
    else if (strcmp(name, "all") == 0)
        *lit = P15_LIT_ALL;
    else if (strcmp(name, "dark") == 0)
        *lit = P15_LIT_NONE;
    else
        return false;
    return true;
}

static bool number_of(const char *s, uint32_t lo, uint32_t hi, uint32_t *out)
{
    if (s == NULL || out == NULL)
        return false;
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 10);
    if (end == s || end == NULL || *end != '\0')
        return false;
    if (v < lo || v > hi)
        return false;
    *out = (uint32_t)v;
    return true;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fputs(USAGE, stderr);
        return EXIT_USAGE;
    }

    const char *cmd = argv[1];
    const char *chip = P15_CHIP_PATH;
    const char *state = NULL;
    uint32_t seconds = 10;
    uint32_t cycles = 3;
    bool any_chip = false;
    p15_polarity pol = P15_SOURCING;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--sinking") == 0) {
            pol = P15_SINKING;
        } else if (strcmp(argv[i], "--any-chip") == 0) {
            any_chip = true;
        } else if (strcmp(argv[i], "--chip") == 0 && i + 1 < argc) {
            chip = argv[++i];
        } else if (strcmp(argv[i], "--state") == 0 && i + 1 < argc) {
            state = argv[++i];
        } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            if (!number_of(argv[++i], 1u, 3600u, &seconds)) {
                fprintf(stderr, "p15: --seconds wants 1 to 3600\n");
                return EXIT_USAGE;
            }
        } else if (strcmp(argv[i], "--cycles") == 0 && i + 1 < argc) {
            if (!number_of(argv[++i], 1u, 1000u, &cycles)) {
                fprintf(stderr, "p15: --cycles wants 1 to 1000\n");
                return EXIT_USAGE;
            }
        } else {
            fprintf(stderr, "p15: unknown option %s\n\n", argv[i]);
            fputs(USAGE, stderr);
            return EXIT_USAGE;
        }
    }

    if (strcmp(cmd, "plan") == 0)
        return cmd_plan(pol);
    if (strcmp(cmd, "budget") == 0)
        return cmd_budget();
    if (strcmp(cmd, "wiring") == 0)
        return cmd_wiring();
    if (strcmp(cmd, "probe") == 0)
        return cmd_probe(chip);
    if (strcmp(cmd, "blink") == 0)
        return cmd_blink(cycles, chip, any_chip, pol);
    if (strcmp(cmd, "semaphore") == 0) {
        uint8_t lit = P15_LIT_NONE;
        if (state == NULL) {
            fprintf(stderr, "p15: semaphore wants --state\n\n");
            fputs(USAGE, stderr);
            return EXIT_USAGE;
        }
        if (!state_of(state, &lit)) {
            fprintf(stderr, "p15: --state %s is not one of registered, "
                            "route, event, all, dark\n", state);
            return EXIT_USAGE;
        }
        return cmd_semaphore(lit, seconds, chip, any_chip, pol);
    }

    fprintf(stderr, "p15: unknown command %s\n\n", cmd);
    fputs(USAGE, stderr);
    return EXIT_USAGE;
}
