/* Two formulas, and the sweep that is the acceptance test.
 *
 * The denominators are what make the second half of that test true. Pitch
 * takes ax against the magnitude of ay and az together, so rotating about
 * the x axis moves ay into az and back without changing their combined
 * length at all. Roll is the same the other way. That is why a single-axis
 * rotation leaves the other angle at zero exactly, rather than nearly.
 */

#include "p19.h"

#include <math.h>
#include <string.h>

const char *p19_strerror(p19_status s)
{
    switch (s) {
    case P19_OK:       return "ok";
    case P19_E_IO:     return "the display could not be reached";
    case P19_E_FORMAT: return "the content is not in the expected form";
    case P19_E_RANGE:  return "a value is outside the range this program allows";
    case P19_E_ARG:    return "an argument is missing or will not do";
    }
    return "unknown";
}

/* The same guard P04 uses, and for the same reason: under -std=c11 the
 * strict ISO mode hides this, so a file that assumes it compiles on one
 * machine and not the next. */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double degrees_of(double radians)
{
    return radians * 180.0 / M_PI;
}

void p19_angles_of(p19_attitude *out, double ax, double ay, double az)
{
    if (out == NULL)
        return;
    out->pitch_deg = degrees_of(atan2(ax, sqrt(ay * ay + az * az)));
    out->roll_deg  = degrees_of(atan2(ay, sqrt(ax * ax + az * az)));
}

void p19_angles(const adxl345_sample *s, p19_attitude *out)
{
    if (s == NULL || out == NULL)
        return;
    p19_angles_of(out, s->ax, s->ay, s->az);
}

/* One gravity, rotated about a single axis by the given angle. This is the
 * reading a still assembly gives when it is tilted and nothing else is
 * happening to it, which is the condition the acceptance test is run under. */
static void gravity_at(p19_axis axis, double deg, adxl345_sample *s)
{
    const double r = deg * M_PI / 180.0;
    memset(s, 0, sizeof *s);
    if (axis == P19_AXIS_PITCH) {
        s->ax = sin(r);
        s->az = cos(r);
    } else {
        s->ay = sin(r);
        s->az = cos(r);
    }
    s->magnitude = sqrt(s->ax * s->ax + s->ay * s->ay + s->az * s->az);
}

p19_status p19_sweep_axis(p19_axis axis, double from_deg, double to_deg,
                          double step_deg, p19_sweep *out)
{
    if (out == NULL || step_deg <= 0.0 || to_deg < from_deg)
        return P19_E_ARG;

    memset(out, 0, sizeof *out);
    /* Rounded rather than truncated, so that a sweep includes both ends.
     * A range divided by a step lands a hair either side of the whole
     * number it should be, and which side depends on the two values: with
     * these ones it lands above, and a step chosen differently will land
     * below. Truncating would then drop the last reading, which is the one
     * at full scale and the one the acceptance test is about. */
    const uint32_t n = (uint32_t)((to_deg - from_deg) / step_deg + 0.5) + 1u;

    for (uint32_t i = 0; i < n; i++) {
        const double deg = from_deg + (double)i * step_deg;
        adxl345_sample s;
        gravity_at(axis, deg, &s);

        p19_attitude a;
        p19_angles(&s, &a);

        const double swept = (axis == P19_AXIS_PITCH) ? a.pitch_deg : a.roll_deg;
        const double other = (axis == P19_AXIS_PITCH) ? a.roll_deg : a.pitch_deg;

        if (fabs(swept) > fabs(out->reached_deg))
            out->reached_deg = swept;
        if (fabs(other) > fabs(out->worst_crosstalk))
            out->worst_crosstalk = other;
        out->steps++;
    }
    return P19_OK;
}
