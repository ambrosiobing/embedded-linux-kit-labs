/* The suite. No hardware, no bus, no sensor, no clock.
 *
 * Four groups, kept apart because they fail for different reasons:
 *
 *   decode   whether two bytes become an acceleration, sign and all, which
 *            is the half of this that is usually wrong
 *   policy   whether the dead band and the cool-off do what the chapters
 *            claim, measured as counts rather than described
 *   axes     whether the decision survives the sensor being on its side and
 *            in free fall, which is this lab's own claim
 *   budget   what reaches a radio and a soldered flash in an hour, which is
 *            P09's question asked before the run rather than after
 */

#include "../src/p01.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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

static const char *TMP_SCENE = "test_p01.samples";

static p01_sample decoded(const char *text)
{
    uint8_t raw[6] = {0};
    p01_sample s;
    memset(&s, 0, sizeof s);
    if (p01_parse_bytes(text, raw))
        p01_decode(raw, &s);
    return s;
}

/* ---------------------------------------------------------------- groups */

static void test_decode(void)
{
    printf("\n-- decode\n");

    uint8_t raw[6];
    ok(p01_parse_bytes("00 00 00 00 00 01", raw) && raw[5] == 1,
       "six hexadecimal bytes parse");
    ok(p01_parse_bytes("00,00,00,00,00,01", raw) && raw[5] == 1,
       "and so do six separated by commas");
    ok(p01_parse_bytes("0x00 0x00 0x00 0x00 0x00 0x01", raw) && raw[5] == 1,
       "and six written with the prefix");
    ok(p01_parse_bytes("00 00 00 00 00 fe", raw) && raw[5] == 0xfe,
       "and the half of the range that is not also decimal");
    ok(!p01_parse_bytes("00 00 00 00 00", raw), "five values are refused");
    ok(!p01_parse_bytes("00 00 00 00 00 01 02", raw), "and seven are");
    ok(!p01_parse_bytes("00 00 00 00 00 100", raw),
       "a value past a byte is refused rather than truncated");
    ok(!p01_parse_bytes("00 00 00 00 00 zz", raw),
       "and one that is not a number at all");

    /* Flat on a table: one gravity on z, which is 256 counts at full
     * resolution, low byte first. */
    p01_sample s = decoded("00 00 00 00 00 01");
    ok(fabs(s.az - 1.0) < 1e-9, "flat on a table reads one gravity on z");
    ok(fabs(s.magnitude - 1.0) < 1e-9, "and a magnitude of one");

    /* Upside down. This is the one a missed sign extension gets wrong, and
     * it gets it wrong by a factor of two hundred and fifty five. */
    s = decoded("00 00 00 00 00 ff");
    ok(fabs(s.az + 1.0) < 1e-9, "upside down reads minus one gravity");
    ok(fabs(s.magnitude - 1.0) < 1e-9,
       "whose magnitude is still one, which is what makes it not an event");

    s = decoded("00 02 00 00 00 00");
    ok(fabs(s.ax - 2.0) < 1e-9, "0x0200 on x is two gravities");
    s = decoded("00 fe 00 00 00 00");
    ok(fabs(s.ax + 2.0) < 1e-9, "and 0xFE00 is minus two");

    s = decoded("01 00 00 00 00 00");
    ok(fabs(s.ax - 1.0 / 256.0) < 1e-12,
       "one count is one 256th of a gravity, which is what full resolution means");

    s = decoded("00 00 00 00 00 00");
    ok(fabs(s.magnitude) < 1e-12, "all zeroes is free fall, not rest");
}

static void test_policy(void)
{
    printf("\n-- policy\n");

    p01_policy p;
    p01_sample rest = decoded("00 00 00 00 00 01");
    p01_sample hit  = decoded("00 00 00 00 00 02");

    p01_policy_init(&p, P01_DEADBAND_G, P01_COOLOFF_S);
    bool any = false;
    for (int i = 0; i < 200; i++)
        any |= p01_policy_offer(&p, (double)i * 0.05, &rest);
    ok(!any, "no sample of a still table is an event");
    ok(p.events == 0 && p.over_threshold == 0,
       "and ten seconds of one produces nothing at all");

    /* A table that never stops ringing, for ten seconds at twenty hertz.
     * Two hundred samples past the dead band, and the cool-off is the only
     * thing between them and two hundred publishes. */
    p01_policy_init(&p, P01_DEADBAND_G, P01_COOLOFF_S);
    for (int i = 0; i < 200; i++)
        p01_policy_offer(&p, (double)i * 0.05, &hit);
    ok(p.over_threshold == 200, "all two hundred samples are past the dead band");
    ok(p.events == 5,
       "and five are published, one every two seconds, which is the whole "
       "point of the cool-off");
    ok(p.suppressed == 195, "the other hundred and ninety five are suppressed");

    /* The cool-off is a parameter, so it has to parameterise. */
    p01_policy_init(&p, P01_DEADBAND_G, 1.0);
    for (int i = 0; i < 200; i++)
        p01_policy_offer(&p, (double)i * 0.05, &hit);
    ok(p.events == 10, "a one second cool-off publishes ten times in ten seconds");

    /* The dead band is the other parameter. */
    p01_policy_init(&p, 1.5, P01_COOLOFF_S);
    for (int i = 0; i < 200; i++)
        p01_policy_offer(&p, (double)i * 0.05, &hit);
    ok(p.events == 0,
       "a dead band wider than the knock lets nothing through, which is how "
       "a threshold set by hope rather than by measurement behaves");
}

static void test_axes(void)
{
    printf("\n-- the axes, which is this lab's own claim\n");

    p01_policy p;

    /* The sensor lying on its side. One gravity, on x instead of z. A
     * threshold on a single axis calls this an event every time the board
     * is picked up and set down differently. */
    p01_sample side = decoded("00 01 00 00 00 00");
    ok(fabs(side.ax - 1.0) < 1e-9 && fabs(side.az) < 1e-12,
       "on its side, the gravity has moved to another axis");
    ok(fabs(side.magnitude - 1.0) < 1e-9, "and the magnitude has not changed");
    p01_policy_init(&p, P01_DEADBAND_G, P01_COOLOFF_S);
    p01_policy_offer(&p, 0.0, &side);
    ok(p.events == 0 && p.over_threshold == 0,
       "so nothing is published, because nothing happened");

    /* Free fall. Zero gravities is as far from rest as two is, in the other
     * direction, and a comparison on the distance from one gets that right
     * without a second rule. */
    p01_sample fall = decoded("00 00 00 00 00 00");
    p01_policy_init(&p, P01_DEADBAND_G, P01_COOLOFF_S);
    ok(p01_policy_offer(&p, 0.0, &fall),
       "free fall is an event, and it is an event in the direction a "
       "threshold on the high side would miss");

    /* And the one in between: half a gravity of extra, which is past the
     * dead band of 0.35 but is not a knock. The band is where the lab put
     * it, not where it feels right. */
    p01_sample nudge = decoded("00 00 00 00 80 01");
    ok(fabs(nudge.magnitude - 1.5) < 1e-9, "a nudge reads one and a half");
    p01_policy_init(&p, P01_DEADBAND_G, P01_COOLOFF_S);
    ok(p01_policy_offer(&p, 0.0, &nudge), "which is past the dead band");
    p01_policy_init(&p, 0.6, P01_COOLOFF_S);
    ok(!p01_policy_offer(&p, 0.0, &nudge), "and inside a wider one");
}

static void test_budget(void)
{
    printf("\n-- the budget, which is P09's question\n");

    p01_budget b;
    ok(p01_budget_make(&b, 20.0, 2.0, 30.0, 48, 1.0) == P01_OK,
       "an hour at twenty hertz with a two second cool-off");
    ok(fabs(b.samples - 72000.0) < 1e-6, "is seventy two thousand samples");
    ok(fabs(b.events - 1800.0) < 1e-6,
       "and at most eighteen hundred events, whatever the table does");
    ok(fabs(b.compression - 40.0) < 1e-6,
       "which is forty times fewer writes than publishing every sample");
    ok(fabs(b.bytes_to_flash - 86400.0) < 1e-6,
       "eighty six thousand bytes to the flash rather than three and a half "
       "million");
    ok(fabs(b.fsyncs - 120.0) < 1e-6, "a hundred and twenty fsyncs in the hour");
    ok(b.fsyncs_per_event < 1.0,
       "fewer than one per event, which is what a timer rather than a sample "
       "means in numbers");

    /* An fsync interval shorter than the cool-off commits the flash when
     * nothing has been written, which is the mistake this figure exists to
     * make visible. */
    ok(p01_budget_make(&b, 20.0, 60.0, 1.0, 48, 1.0) == P01_OK,
       "a one second fsync beside a one minute cool-off");
    ok(b.fsyncs_per_event > 1.0,
       "is more than one fsync per event, which is a write to flash with "
       "nothing new in it");

    ok(p01_budget_make(&b, 0.0, 2.0, 30.0, 48, 1.0) != P01_OK,
       "a rate of zero is refused");
    ok(p01_budget_make(&b, 20.0, 2.0, 30.0, 48, 0.0) != P01_OK,
       "and so is an hour that is not an hour");
}

static void test_scenes(void)
{
    printf("\n-- the scenes, end to end\n");

    p01_run r;
    ok(p01_scene_write(P01_SCENE_AT_REST, 10.0, 20.0, TMP_SCENE) == P01_OK,
       "ten seconds of a still bench");
    ok(p01_replay(TMP_SCENE, P01_DEADBAND_G, P01_COOLOFF_S, &r) == P01_OK,
       "it replays");
    ok(r.policy.samples == 200 && r.malformed == 0, "two hundred samples, all read");
    ok(r.policy.events == 0, "and nothing published");

    ok(p01_scene_write(P01_SCENE_ON_ITS_SIDE, 10.0, 20.0, TMP_SCENE) == P01_OK,
       "ten seconds with the sensor on its side");
    ok(p01_replay(TMP_SCENE, P01_DEADBAND_G, P01_COOLOFF_S, &r) == P01_OK,
       "it replays");
    ok(r.policy.events == 0,
       "and still nothing, which a per-axis threshold would not manage");

    ok(p01_scene_write(P01_SCENE_RINGING, 10.0, 20.0, TMP_SCENE) == P01_OK,
       "ten seconds of a table that never settles");
    ok(p01_replay(TMP_SCENE, P01_DEADBAND_G, P01_COOLOFF_S, &r) == P01_OK,
       "it replays");
    ok(r.policy.over_threshold == 200 && r.policy.events == 5,
       "two hundred samples past the band, five published");

    ok(p01_scene_write(P01_SCENE_ONE_KNOCK, 10.0, 20.0, TMP_SCENE) == P01_OK,
       "ten seconds with one strike in it");
    ok(p01_replay(TMP_SCENE, P01_DEADBAND_G, P01_COOLOFF_S, &r) == P01_OK,
       "it replays");
    ok(r.policy.over_threshold > 1 && r.policy.events == 1,
       "one fist, one publish, and the ringing behind it suppressed");
    ok(r.peak_magnitude > 2.0, "with a peak well past the dead band");
}

static void cleanup(void)
{
    remove(TMP_SCENE);
}

int main(void)
{
    printf("P01 suite: no hardware, no bus, no sensor, no clock\n");

    test_decode();
    test_policy();
    test_axes();
    test_budget();
    test_scenes();
    cleanup();

    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    if (g_failed > 0)
        return 1;
    printf("No acceleration here was measured. Every sample came from six\n"
           "bytes this suite wrote a moment earlier.\n");
    return 0;
}
