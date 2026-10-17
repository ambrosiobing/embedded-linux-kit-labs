/* P19, the offline tilt meter.
 *
 * Six bytes in, two angles out, two bars on a panel. There is no branch in
 * this program that reaches a network, and that is the chapter's point
 * rather than an omission: an instrument with a network has states for
 * connecting, retrying and giving up, and this one has a read that either
 * returns six bytes or does not, and a draw that either paints or does not.
 * Less reach, far less to go wrong.
 *
 * Nothing here is new. The sensor is lib/adxl345.h, which P01 and P09 also
 * read, and the panel is lib/framebuffer.h, which P06 also paints. What
 * belongs to this lab is two formulas and a drawing, which is exactly what
 * the chapter says the product is.
 *
 * The acceptance test is the interesting part, and it is two statements
 * rather than one. Rotate the assembly ninety degrees on one axis and that
 * bar travels full scale. **And the other bar stays near zero.** The second
 * half is what the two-argument arctangent and the choice of denominator
 * are for, and it is the half a per-axis instrument gets wrong.
 *
 * One property is written down here because it is a limit rather than a
 * defect. Both formulas take the arctangent against a positive square root,
 * so both fold at ninety degrees: a hundred and twenty degrees of pitch
 * reads the same as sixty. The acceptance test asks for ninety, so the range
 * is exactly enough and no more, and a reader who wants a full turn needs a
 * different instrument rather than a wider scale.
 */
#ifndef P19_H
#define P19_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../../lib/adxl345.h"
#include "../../lib/framebuffer.h"

#define P19_FULL_SCALE_DEG 90.0
#define P19_SAMPLE_HZ 20.0

/* The colours of the two bars and their furniture, as 0xRRGGBB. */
#define P19_COLOUR_BACKGROUND 0x000000u
#define P19_COLOUR_TRACK      0x202020u
#define P19_COLOUR_CENTRE     0x606060u
#define P19_COLOUR_PITCH      0x00C0FFu
#define P19_COLOUR_ROLL       0xFFC000u

typedef enum {
    P19_OK = 0,
    P19_E_IO,
    P19_E_FORMAT,
    P19_E_RANGE,
    P19_E_ARG
} p19_status;

const char *p19_strerror(p19_status s);

/* ---------------------------------------------------------------- angles */

typedef struct {
    double pitch_deg;
    double roll_deg;
} p19_attitude;

/* Pitch is the arctangent of ax against the magnitude of the other two, and
 * roll is the same with ay. Both use the two-argument form so the sign
 * survives, and both fold at ninety degrees, which the header explains. */
void p19_angles(const adxl345_sample *s, p19_attitude *out);
/* The same thing from three accelerations, for callers that have the
 * numbers rather than a sample: the suite tilts a gravity by hand and the
 * sweep does the same. */
void p19_angles_of(p19_attitude *out, double ax, double ay, double az);

/* The acceptance test, computed. Rotating about one axis must move that
 * angle full scale and leave the other where it was, and this reports the
 * largest the other one reached across a sweep. */
typedef enum { P19_AXIS_PITCH = 0, P19_AXIS_ROLL } p19_axis;

typedef struct {
    double reached_deg;        /* the largest the swept angle reached */
    double worst_crosstalk;    /* the largest the other one reached */
    uint32_t steps;
} p19_sweep;

p19_status p19_sweep_axis(p19_axis axis, double from_deg, double to_deg,
                          double step_deg, p19_sweep *out);

/* ---------------------------------------------------------------- the bars */

typedef struct {
    uint32_t x, y, w, h;
} p19_rect;

/* Two tracks, one in each half of the panel, with a margin that scales with
 * the panel rather than a number that happens to suit 480 by 320. */
p19_status p19_bar_track(const fb_geometry *g, p19_axis axis, p19_rect *out);
/* Where the marker sits for an angle, clamped to the track. Full scale at
 * the ends, centre at zero, and an angle past full scale rests on the end
 * rather than running off it. */
uint32_t p19_marker_x(const p19_rect *track, double degrees, double full_scale);

p19_status p19_paint(fb_target *fb, const p19_attitude *a);

#endif /* P19_H */
