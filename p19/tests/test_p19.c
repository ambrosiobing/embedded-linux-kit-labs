/* The suite. No hardware, no sensor, no panel, no network.
 *
 * Four groups, kept apart because they fail for different reasons:
 *
 *   angles     whether the two formulas read a tilted gravity correctly,
 *              and whether they fold where the header says they fold
 *   crosstalk  whether a rotation about one axis leaves the other angle
 *              alone, which is the half of the acceptance test a per-axis
 *              instrument fails
 *   bars       whether the marker sits where the angle says, at the ends
 *              and in the middle, and whether it stays on the track
 *   glass      whether anything is ever written outside the panel
 */
#define _POSIX_C_SOURCE 200809L

#include "../src/p19.h"

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

static const char *TMP_GLASS = "test_p19.glass";
#define GUARD_BYTES 256
#define GUARD_FILL 0xAA

static bool make_glass(const char *path, size_t bytes, size_t guard)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return false;
    for (size_t i = 0; i < bytes + guard; i++)
        if (fputc(GUARD_FILL, fp) == EOF) {
            fclose(fp);
            return false;
        }
    return fclose(fp) == 0;
}

static bool guard_intact(const char *path, size_t bytes, size_t guard)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return false;
    if (fseek(fp, (long)bytes, SEEK_SET) != 0) {
        fclose(fp);
        return false;
    }
    bool good = true;
    for (size_t i = 0; i < guard; i++)
        if (fgetc(fp) != GUARD_FILL) {
            good = false;
            break;
        }
    fclose(fp);
    return good;
}

/* ---------------------------------------------------------------- groups */

static void test_angles(void)
{
    printf("\n-- angles\n");

    p19_attitude a;
    p19_angles_of(&a, 0.0, 0.0, 1.0);
    ok(fabs(a.pitch_deg) < 1e-9 && fabs(a.roll_deg) < 1e-9,
       "flat on a table is zero pitch and zero roll");

    p19_angles_of(&a, 1.0, 0.0, 0.0);
    ok(fabs(a.pitch_deg - 90.0) < 1e-9, "all the gravity on x is ninety of pitch");
    ok(fabs(a.roll_deg) < 1e-9, "and no roll at all");

    p19_angles_of(&a, 0.0, 1.0, 0.0);
    ok(fabs(a.roll_deg - 90.0) < 1e-9, "all of it on y is ninety of roll");
    ok(fabs(a.pitch_deg) < 1e-9, "and no pitch");

    p19_angles_of(&a, -1.0, 0.0, 0.0);
    ok(fabs(a.pitch_deg + 90.0) < 1e-9, "the other way is minus ninety");

    /* Forty five degrees about the y axis: equal parts x and z. */
    const double h = sqrt(0.5);
    p19_angles_of(&a, h, 0.0, h);
    ok(fabs(a.pitch_deg - 45.0) < 1e-9, "equal parts x and z is forty five");

    /* The fold the header promises. Past ninety degrees the denominator is
     * a positive square root, so a hundred and twenty reads as sixty. This
     * is a limit of the instrument, not a defect, and it is checked so that
     * nobody later reads it as one. */
    p19_angles_of(&a, sin(120.0 * 3.14159265358979323846 / 180.0), 0.0,
                         cos(120.0 * 3.14159265358979323846 / 180.0));
    ok(fabs(a.pitch_deg - 60.0) < 1e-6,
       "a hundred and twenty degrees reads as sixty, which is the fold at "
       "ninety and the reason this instrument stops there");
}

static void test_crosstalk(void)
{
    printf("\n-- crosstalk, which is the other half of the acceptance test\n");

    p19_sweep sw;
    ok(p19_sweep_axis(P19_AXIS_PITCH, -90.0, 90.0, 1.0, &sw) == P19_OK,
       "pitch is swept from minus ninety to plus ninety");
    ok(sw.steps == 181, "in a hundred and eighty one steps");
    ok(fabs(fabs(sw.reached_deg) - 90.0) < 1e-9, "pitch reaches full scale");
    ok(fabs(sw.worst_crosstalk) < 1e-9,
       "and roll never moves at all, which is exact rather than close");

    ok(p19_sweep_axis(P19_AXIS_ROLL, -90.0, 90.0, 1.0, &sw) == P19_OK,
       "roll is swept the same way");
    ok(fabs(fabs(sw.reached_deg) - 90.0) < 1e-9, "roll reaches full scale");
    ok(fabs(sw.worst_crosstalk) < 1e-9, "and pitch never moves");

    /* A finer sweep, in case the coarse one stepped over something. */
    ok(p19_sweep_axis(P19_AXIS_PITCH, -90.0, 90.0, 0.1, &sw) == P19_OK,
       "and again in tenths of a degree");
    ok(sw.steps == 1801, "which is eighteen hundred and one readings");
    ok(fabs(sw.worst_crosstalk) < 1e-9, "with the same answer");

    ok(p19_sweep_axis(P19_AXIS_PITCH, -90.0, 90.0, 0.0, &sw) != P19_OK,
       "a step of zero is refused rather than looping");
}

static void test_bars(void)
{
    printf("\n-- the bars\n");

    fb_geometry g = {480, 320, 16};
    p19_rect pitch;
    p19_rect roll;
    ok(p19_bar_track(&g, P19_AXIS_PITCH, &pitch) == P19_OK, "the pitch track is laid out");
    ok(p19_bar_track(&g, P19_AXIS_ROLL, &roll) == P19_OK, "and the roll track");
    ok(pitch.y + pitch.h <= roll.y, "the two do not overlap");
    ok(pitch.x + pitch.w <= g.width && roll.y + roll.h <= g.height,
       "and both are inside the panel");

    const uint32_t centre = p19_marker_x(&pitch, 0.0, P19_FULL_SCALE_DEG);
    const uint32_t left = p19_marker_x(&pitch, -90.0, P19_FULL_SCALE_DEG);
    const uint32_t right = p19_marker_x(&pitch, 90.0, P19_FULL_SCALE_DEG);
    ok(centre == pitch.x + pitch.w / 2u, "zero degrees sits at the centre");
    ok(left == pitch.x, "minus full scale sits at the left end");
    ok(right == pitch.x + pitch.w - 1u, "and plus full scale at the right end");

    ok(p19_marker_x(&pitch, 180.0, P19_FULL_SCALE_DEG) == right,
       "an angle past full scale rests on the end rather than running off it");
    ok(p19_marker_x(&pitch, -180.0, P19_FULL_SCALE_DEG) == left,
       "and the same at the other end");

    const uint32_t half = p19_marker_x(&pitch, 45.0, P19_FULL_SCALE_DEG);
    ok(half > centre && half < right, "forty five sits between the two");

    /* A panel too small for two readable tracks is refused rather than
     * drawn one pixel high. */
    fb_geometry tiny = {16, 8, 16};
    ok(p19_bar_track(&tiny, P19_AXIS_PITCH, &pitch) != P19_OK,
       "a panel too small for the drawing is refused");
}

static void test_glass(void)
{
    printf("\n-- the glass\n");

    fb_geometry g = {160, 128, 16};
    const size_t bytes = 160u * 128u * 2u;
    ok(make_glass(TMP_GLASS, bytes, GUARD_BYTES),
       "a glass file with a guard area is made");

    fb_target fb;
    fb_init(&fb);
    ok(fb_open(&fb, TMP_GLASS, &g) == FB_OK, "it opens");

    p19_attitude a;
    a.pitch_deg = 90.0;
    a.roll_deg = -90.0;
    ok(p19_paint(&fb, &a) == P19_OK, "both bars at their ends paint");
    ok(guard_intact(TMP_GLASS, bytes, GUARD_BYTES),
       "and nothing was written past the panel");

    a.pitch_deg = 4000.0;
    a.roll_deg = -4000.0;
    ok(p19_paint(&fb, &a) == P19_OK, "an absurd angle paints");
    ok(guard_intact(TMP_GLASS, bytes, GUARD_BYTES),
       "and still nothing past the panel, because the marker is clamped and "
       "the fill is clipped");
    ok(fb_close(&fb) == FB_OK, "the glass closes");

    /* A panel this program will not draw on. */
    fb_geometry tiny = {16, 8, 16};
    const size_t tiny_bytes = 16u * 8u * 2u;
    ok(make_glass(TMP_GLASS, tiny_bytes, GUARD_BYTES), "a tiny glass is made");
    fb_init(&fb);
    ok(fb_open(&fb, TMP_GLASS, &tiny) == FB_OK, "it opens");
    ok(p19_paint(&fb, &a) != P19_OK,
       "and the drawing is refused rather than squeezed onto it");
    ok(fb_close(&fb) == FB_OK, "it closes");
}

static void cleanup(void)
{
    remove(TMP_GLASS);
}

int main(void)
{
    printf("P19 suite: no hardware, no sensor, no panel, no network\n");

    test_angles();
    test_crosstalk();
    test_bars();
    test_glass();
    cleanup();

    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    if (g_failed > 0)
        return 1;
    printf("No assembly was rotated. Every angle came from a gravity this\n"
           "suite invented a moment earlier.\n");
    return 0;
}
