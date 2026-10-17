/* The suite. No chip, no board, no LEDs, and no clock.
 *
 * Six groups, kept apart because they fail for different reasons:
 *
 *   wiring     whether the channel order is the one the kernel's masks are
 *              indexed by, and whether the chip guard is a guard
 *   polarity   whether a lit set becomes the right line values under both
 *              arrangements, over all eight lit sets rather than a sample
 *   plan       whether the sequence is the sequence, at each boundary
 *              millisecond, and whether it ends dark
 *   crossed    whether the single-channel steps are what detects a crossed
 *              pair of jumpers, which is the whole argument for a program
 *              rather than one command that lights all three
 *   budget     whether the measured table adds up, and which triple each
 *              published figure belongs to
 *   device     whether the device half refuses what it cannot do, which is
 *              the only part of it a machine with no header can exercise
 *
 * Nothing here sleeps. The plan takes its time as an argument, so three
 * seconds of blinking is tested in microseconds and every step boundary is
 * tested exactly instead of approximately.
 */

#include "../src/p15.h"

#include <stdio.h>
#include <string.h>

static int g_checks = 0;
static int g_failed = 0;

static void ok(bool cond, const char *what)
{
    g_checks++;
    if (cond) {
        printf("ok   %s\n", what);
    } else {
        g_failed++;
        printf("FAIL %s\n", what);
    }
}

/* --------------------------------------------------------------- wiring */

static void test_wiring(void)
{
    printf("\n-- wiring, and the order the kernel's masks are indexed by\n");

    const p15_wire *w = p15_wiring();

    /* This is not decoration. Every mask and every bits field handed to the
     * GPIO ioctls is indexed by a line's position in the request's offsets
     * array, and lines.c fills that array in this order. If the order here
     * changed, a lit set's bits would reach the wrong lines and every test
     * above this one would still pass. */
    ok(strcmp(w[P15_GREEN].colour, "green") == 0,
       "channel 0 is green, which is the bit the kernel sees first");
    ok(strcmp(w[P15_YELLOW].colour, "yellow") == 0, "channel 1 is yellow");
    ok(strcmp(w[P15_RED].colour, "red") == 0, "channel 2 is red");

    ok(strcmp(w[P15_GREEN].meaning, "registered") == 0,
       "green means registered, as it does in P01, P02 and P03");
    ok(strcmp(w[P15_YELLOW].meaning, "a default route") == 0,
       "yellow means a default route");
    ok(strcmp(w[P15_RED].meaning, "an event") == 0, "red means an event");

    /* The dated observation of Friday 2 October 2026. */
    ok(w[P15_GREEN].header_pin == 11u && w[P15_GREEN].offset == 17u,
       "green is header pin 11, BCM 17");
    ok(w[P15_YELLOW].header_pin == 13u && w[P15_YELLOW].offset == 27u,
       "yellow is header pin 13, BCM 27");
    ok(w[P15_RED].header_pin == 15u && w[P15_RED].offset == 22u,
       "red is header pin 15, BCM 22");

    ok(p15_wire_of(0u) != NULL && p15_wire_of(2u) != NULL,
       "the three channels have wires");
    ok(p15_wire_of(3u) == NULL && p15_wire_of(99u) == NULL,
       "and a fourth channel has none, rather than a plausible one");

    printf("\n   the chip guard\n");
    ok(p15_chip_matches("pinctrl-bcm2835"),
       "the chip this lab's offsets were recorded on is accepted");
    ok(!p15_chip_matches("pinctrl-rp1"),
       "a Pi 5's chip is refused, because BCM 17 is not line 17 there");
    ok(!p15_chip_matches("pinctrl-bcm2835-and-a-bit"),
       "a label that merely starts the same is refused, so this is a "
       "comparison and not a prefix");
    ok(!p15_chip_matches(""), "an empty label is refused");
    ok(!p15_chip_matches(NULL), "and so is no label at all");
}

/* ------------------------------------------------------------- polarity */

static void test_polarity(void)
{
    printf("\n-- polarity, over all eight lit sets rather than a sample\n");

    /* Sourcing is what Friday 2 October 2026 measured: high is lit. */
    ok(p15_bits_for(P15_LIT_NONE, P15_SOURCING) == 0x0u,
       "sourcing: dark is all three lines low");
    ok(p15_bits_for(P15_LIT_ALL, P15_SOURCING) == 0x7u,
       "sourcing: three lit is all three lines high");
    ok(p15_bits_for(P15_LIT(P15_YELLOW), P15_SOURCING) == 0x2u,
       "sourcing: yellow alone is the middle line");

    ok(p15_bits_for(P15_LIT_NONE, P15_SINKING) == 0x7u,
       "sinking: dark is all three lines high");
    ok(p15_bits_for(P15_LIT_ALL, P15_SINKING) == 0x0u,
       "sinking: three lit is all three lines low");
    ok(p15_bits_for(P15_LIT(P15_YELLOW), P15_SINKING) == 0x5u,
       "sinking: yellow alone leaves the other two high");

    unsigned round_trips = 0;
    unsigned inversions = 0;
    for (unsigned m = 0; m <= P15_LIT_ALL; m++) {
        uint8_t lit = (uint8_t)m;
        uint8_t src = p15_bits_for(lit, P15_SOURCING);
        uint8_t snk = p15_bits_for(lit, P15_SINKING);

        if (p15_lit_from(src, P15_SOURCING) == lit &&
            p15_lit_from(snk, P15_SINKING) == lit)
            round_trips++;
        if (src == (uint8_t)(~(unsigned)snk & P15_LIT_ALL))
            inversions++;
    }
    ok(round_trips == 8u,
       "all eight lit sets survive the trip out to line values and back, "
       "under both arrangements");
    ok(inversions == 8u,
       "and the two arrangements are exact inversions of each other on "
       "every one of them");

    ok(p15_bits_for(0xF8u, P15_SOURCING) == 0x7u,
       "a lit set with rubbish in the high bits drives three lines and not "
       "a fourth");
    ok(p15_dark_bits(P15_SOURCING) == 0x0u &&
       p15_dark_bits(P15_SINKING) == 0x7u,
       "the dark pattern follows the arrangement, which is what the line "
       "request carries as its first value");
}

/* ----------------------------------------------------------------- plan */

static void test_plan(void)
{
    printf("\n-- the plan, at every boundary millisecond\n");

    ok(P15_PLAN_MS == 3000u, "a cycle is three seconds");
    ok(P15_STEPS == 5u, "in five steps");

    ok(p15_frame_at(0) == P15_LIT(P15_GREEN), "at 0 ms, green alone");
    ok(p15_frame_at(599) == P15_LIT(P15_GREEN), "at 599 ms, still green");
    ok(p15_frame_at(600) == P15_LIT(P15_YELLOW), "at 600 ms, yellow alone");
    ok(p15_frame_at(1199) == P15_LIT(P15_YELLOW), "at 1199 ms, still yellow");
    ok(p15_frame_at(1200) == P15_LIT(P15_RED), "at 1200 ms, red alone");
    ok(p15_frame_at(1799) == P15_LIT(P15_RED), "at 1799 ms, still red");

    ok(p15_frame_at(1800) == P15_LIT_ALL,
       "at 1800 ms, all three, which is the acceptance item");
    ok(p15_frame_at(2399) == P15_LIT_ALL,
       "and they stay lit together for the whole step, which is what a "
       "reader needs to see it");

    ok(p15_frame_at(2400) == P15_LIT_NONE, "at 2400 ms, dark");
    ok(p15_frame_at(2999) == P15_LIT_NONE, "at 2999 ms, still dark");
    ok(p15_frame_at(3000) == P15_LIT(P15_GREEN),
       "at 3000 ms the cycle has come round and green is alone again");

    /* Each channel alone, exactly once, before the three together. That is
     * the property the sequence exists for, so it is asserted rather than
     * left to the reader of the table. */
    unsigned alone_before_all = 0;
    bool seen[P15_CHANNELS] = { false, false, false };
    bool all_seen = false;
    for (unsigned i = 0; i < P15_STEPS; i++) {
        uint8_t lit = p15_step_lit(i);
        if (lit == P15_LIT_ALL) {
            all_seen = true;
            continue;
        }
        for (unsigned ch = 0; ch < P15_CHANNELS; ch++) {
            if (lit == P15_LIT(ch) && !all_seen && !seen[ch]) {
                seen[ch] = true;
                alone_before_all++;
            }
        }
    }
    ok(alone_before_all == 3u,
       "each of the three channels is lit alone, once, before the step that "
       "lights all three");
    ok(all_seen, "and that step is in the plan");

    ok(p15_step_lit(P15_STEPS - 1u) == P15_LIT_NONE,
       "the last step is dark, so a cycle leaves the semaphore in the state "
       "the next lab needs to find");

    ok(p15_step_lit(P15_STEPS) == P15_LIT_NONE &&
       p15_step_lit(4000u) == P15_LIT_NONE,
       "a step index past the end is dark rather than whatever is next in "
       "memory");

    unsigned long covered = 0;
    for (unsigned t = 0; t < P15_PLAN_MS; t++) {
        if (p15_frame_at(t) == p15_step_lit(p15_step_at(t)))
            covered++;
    }
    ok(covered == P15_PLAN_MS,
       "and every millisecond of a cycle agrees with the step it falls in, "
       "all three thousand of them");
}

/* -------------------------------------------------------------- crossed */

/* Swap the green and yellow bits, which is what two crossed jumpers do to
 * what a reader sees. */
static uint8_t swap_green_yellow(uint8_t lit)
{
    uint8_t g = (uint8_t)(lit & P15_LIT(P15_GREEN));
    uint8_t y = (uint8_t)(lit & P15_LIT(P15_YELLOW));
    uint8_t rest = (uint8_t)(lit & ~(unsigned)(P15_LIT(P15_GREEN) |
                                               P15_LIT(P15_YELLOW)));
    return (uint8_t)(rest | (g != 0 ? P15_LIT(P15_YELLOW) : 0u) |
                     (y != 0 ? P15_LIT(P15_GREEN) : 0u));
}

static void test_crossed(void)
{
    printf("\n-- the crossed pair, which is the argument for the sequence\n");

    ok(swap_green_yellow(P15_LIT(P15_GREEN)) == P15_LIT(P15_YELLOW) &&
       swap_green_yellow(P15_LIT(P15_YELLOW)) == P15_LIT(P15_GREEN),
       "the swap is a swap, so this group is testing the plan and not "
       "itself");

    /* The step the acceptance item names cannot tell a crossed pair from a
     * correct one. Three lines high are three lines high in any order. */
    ok(swap_green_yellow(p15_frame_at(1800)) == p15_frame_at(1800),
       "the all-three step is unchanged by a crossed pair, so on its own it "
       "cannot detect one");

    /* The steps before it can, and each of them does. */
    ok(swap_green_yellow(p15_frame_at(0)) != p15_frame_at(0),
       "the green step changes under the swap");
    ok(swap_green_yellow(p15_frame_at(600)) != p15_frame_at(600),
       "and so does the yellow step");

    unsigned detecting = 0;
    for (unsigned i = 0; i < P15_STEPS; i++) {
        if (swap_green_yellow(p15_step_lit(i)) != p15_step_lit(i))
            detecting++;
    }
    ok(detecting == 2u,
       "two of the five steps detect that particular crossed pair, and "
       "neither of them is the one the acceptance item names");

    /* The dark step cannot detect anything either, which is worth stating
     * so that nobody later counts it as evidence. */
    ok(swap_green_yellow(P15_LIT_NONE) == P15_LIT_NONE,
       "the dark step is unchanged too, as a step with nothing lit must be");
}

/* --------------------------------------------------------------- budget */

static void test_budget(void)
{
    printf("\n-- the measured modules, and which triple each figure is from\n");

    const p15_module *blue = p15_module_of("blue");
    const p15_module *green = p15_module_of("green");
    const p15_module *yellow = p15_module_of("yellow");
    const p15_module *red = p15_module_of("red");

    ok(blue != NULL && green != NULL && yellow != NULL && red != NULL,
       "four modules were measured on Saturday 3 October 2026");
    ok(p15_module_of("white") == NULL,
       "and a colour nobody measured has no entry, rather than a zero");
    ok(p15_module_of(NULL) == NULL, "nor does no colour at all");

    if (blue == NULL || green == NULL || yellow == NULL || red == NULL)
        return;

    ok(green->ua_at_3v3 == 7390u && yellow->ua_at_3v3 == 6200u &&
       red->ua_at_3v3 == 6340u,
       "the three channels draw 7.39, 6.20 and 6.34 mA at 3.3 V");

    /* The arithmetic that separates the chapter's figure from this
     * semaphore's. Both are sums of three measured modules; they are sums
     * of different three. */
    uint32_t chapter_triple = blue->ua_at_3v3 + green->ua_at_3v3 +
                              yellow->ua_at_3v3;
    ok(chapter_triple == 16530u,
       "blue, green and yellow together draw 16.53 mA, which is the 16.5 mA "
       "the chapter prints");
    ok(p15_draw_ua(P15_LIT_ALL) == 19930u,
       "green, yellow and red together draw 19.93 mA, which is this "
       "semaphore");
    ok(p15_draw_ua(P15_LIT_ALL) > chapter_triple &&
       p15_draw_ua(P15_LIT_ALL) - chapter_triple == 3400u,
       "the two differ by 3.4 mA, because one triple contains blue and the "
       "other contains red");

    ok(p15_draw_ua(P15_LIT_NONE) == 0u,
       "a dark semaphore draws nothing, rather than a floor");
    ok(p15_draw_ua(P15_LIT(P15_GREEN)) == 7390u,
       "one channel draws its own module");
    ok(p15_draw_ua((uint8_t)(P15_LIT(P15_GREEN) | P15_LIT(P15_RED))) == 13730u,
       "two channels draw the sum of theirs");

    printf("\n   the two limits, which are two different numbers\n");
    ok(p15_peak_channel_ua(P15_LIT_ALL) == 7390u,
       "the largest single channel of the three is green, at 7.39 mA, and "
       "that is the figure a per-pin limit is compared against");
    ok(p15_peak_channel_ua(P15_LIT_ALL) != p15_draw_ua(P15_LIT_ALL),
       "which is not the sum, and the sum is what a whole-header limit is "
       "compared against");
    ok(p15_peak_channel_ua(P15_LIT_NONE) == 0u,
       "and a dark semaphore has no largest channel");

    printf("\n   the formatter, including the carry\n");
    char buf[16];
    ok(p15_format_ma(buf, sizeof buf, 19930u) && strcmp(buf, "19.9 mA") == 0,
       "19930 uA prints as 19.9 mA");
    ok(p15_format_ma(buf, sizeof buf, 16530u) && strcmp(buf, "16.5 mA") == 0,
       "16530 uA prints as 16.5 mA, which is the chapter's own rounding");
    ok(p15_format_ma(buf, sizeof buf, 2940u) && strcmp(buf, "2.9 mA") == 0,
       "2940 uA prints as 2.9 mA");
    ok(p15_format_ma(buf, sizeof buf, 0u) && strcmp(buf, "0.0 mA") == 0,
       "and nothing prints as 0.0 mA");
    ok(p15_format_ma(buf, sizeof buf, 19960u) && strcmp(buf, "20.0 mA") == 0,
       "19960 uA carries into the whole part and prints 20.0 mA, not "
       "19.10 mA");
    ok(p15_format_ma(buf, sizeof buf, 9999u) && strcmp(buf, "10.0 mA") == 0,
       "and so does 9999 uA");
    ok(!p15_format_ma(buf, 4u, 19930u),
       "a buffer too small is refused rather than filled with half a "
       "number");
    ok(!p15_format_ma(NULL, sizeof buf, 1u), "and so is no buffer");
}

/* --------------------------------------------------------------- device */

static const char *MISSING_CHIP = "/dev/gpiochip-p15-no-such-device";

static void test_device(void)
{
    printf("\n-- the device half, which this machine cannot exercise and "
           "can refuse\n");

    /* Either answer is correct and which one depends on the build: a Linux
     * host with the v2 uAPI in its headers says the chip is not there, and
     * anything else says there is no character device to begin with. What
     * is not correct is success. */
    p15_report r;
    p15_status s = p15_probe(MISSING_CHIP, &r);
    printf("     (this build answers: %s)\n", p15_strstatus(s));
    ok(s != P15_OK,
       "probing a chip that does not exist does not succeed");
    ok(s == P15_E_NO_CHIP || s == P15_E_PLATFORM,
       "and it fails by naming the chip or the platform, not by another "
       "route");
    ok(r.chip_label[0] == '\0' && r.chip_lines == 0u,
       "a failed probe leaves no label and no line count behind");
    ok(!r.used[P15_GREEN] && !r.used[P15_YELLOW] && !r.used[P15_RED],
       "and reports no line as held, rather than guessing");
    ok(p15_probe(MISSING_CHIP, NULL) == P15_E_ARG,
       "a probe with nowhere to put the report is a caller's mistake");

    p15_lines l;
    s = p15_lines_open(&l, MISSING_CHIP, false, P15_SOURCING);
    ok(s != P15_OK, "nor can the lines be opened on a chip that is not there");
    ok(s == P15_E_NO_CHIP || s == P15_E_PLATFORM,
       "and that failure is named the same way");
    ok(l.line_fd < 0 && l.chip_fd < 0,
       "a failed open leaves no descriptor behind to be closed twice");
    ok(p15_lines_open(NULL, MISSING_CHIP, false, P15_SOURCING) == P15_E_ARG,
       "an open with nowhere to put the result is a caller's mistake");

    ok(p15_lines_set(&l, P15_LIT_ALL) == P15_E_ARG,
       "setting values on lines that were never opened is refused, and does "
       "not reach a kernel");
    ok(p15_lines_set(NULL, P15_LIT_ALL) == P15_E_ARG,
       "and so is setting values on nothing");

    p15_lines_close(&l);
    p15_lines_close(&l);
    p15_lines_close(NULL);
    ok(true, "closing a failed open twice, and closing nothing, are both "
             "survivable");

    printf("\n   every status says something, and says it only once\n");
    const p15_status ALL[] = {
        P15_OK, P15_E_ARG, P15_E_NO_CHIP, P15_E_CHIP_INFO, P15_E_WRONG_CHIP,
        P15_E_BUSY, P15_E_REQUEST, P15_E_SET, P15_E_PLATFORM
    };
    const unsigned n = (unsigned)(sizeof ALL / sizeof ALL[0]);
    unsigned named = 0;
    unsigned distinct = 0;
    for (unsigned i = 0; i < n; i++) {
        const char *a = p15_strstatus(ALL[i]);
        if (a != NULL && a[0] != '\0')
            named++;
        bool clash = false;
        for (unsigned j = 0; j < n; j++) {
            if (i != j && strcmp(a, p15_strstatus(ALL[j])) == 0)
                clash = true;
        }
        if (!clash)
            distinct++;
    }
    ok(named == n, "all nine statuses have a sentence");
    ok(distinct == n,
       "and no two share one, so a message identifies which failure it was");
}

int main(void)
{
    printf("P15 suite: no chip, no board, no LEDs, no clock\n");

    test_wiring();
    test_polarity();
    test_plan();
    test_crossed();
    test_budget();
    test_device();

    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    if (g_failed > 0)
        return 1;
    printf("Nothing was lit. The plan was asked what it would do at three\n"
           "thousand separate milliseconds, and the only chip this suite\n"
           "opened was one that does not exist.\n");
    return 0;
}
