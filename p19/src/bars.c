/* The drawing: two tracks, two centre marks, two markers.
 *
 * Everything that touches the mapping goes through lib/framebuffer.h, which
 * clips. The arithmetic here is allowed to be simple because that one is
 * not, and the marker is clamped in any case: an angle past full scale
 * rests on the end of its track rather than running off it, which is a
 * reading rather than a drawing accident.
 */

#include "p19.h"

#include <string.h>

p19_status p19_bar_track(const fb_geometry *g, p19_axis axis, p19_rect *out)
{
    if (g == NULL || out == NULL)
        return P19_E_ARG;
    if (g->width < 40 || g->height < 24)
        return P19_E_RANGE;   /* a panel too small for two readable tracks */

    /* Margins as a fraction of the panel rather than numbers that happen to
     * suit 480 by 320. The 3.5 inch panel is the one in the kit, and it is
     * not the only one this will ever meet. */
    const uint32_t margin_x = g->width / 10u;
    const uint32_t track_h = g->height / 8u;

    memset(out, 0, sizeof *out);
    out->x = margin_x;
    out->w = g->width - 2u * margin_x;
    out->h = track_h;
    /* The pitch track sits a third of the way down and the roll track two
     * thirds, so the two are separated by more than their own height at any
     * panel size. */
    out->y = (axis == P19_AXIS_PITCH) ? (g->height / 3u) - track_h / 2u
                                      : (2u * g->height / 3u) - track_h / 2u;
    return P19_OK;
}

uint32_t p19_marker_x(const p19_rect *track, double degrees, double full_scale)
{
    if (track == NULL || full_scale <= 0.0)
        return 0;

    double fraction = degrees / full_scale;      /* minus one to plus one */
    if (fraction > 1.0)
        fraction = 1.0;
    if (fraction < -1.0)
        fraction = -1.0;

    const double half = (double)track->w / 2.0;
    const double centre = (double)track->x + half;
    double x = centre + fraction * half;

    /* The marker has width, so the last pixel of the track is the furthest
     * its left edge may sit. */
    const double limit = (double)(track->x + track->w - 1u);
    if (x > limit)
        x = limit;
    if (x < (double)track->x)
        x = (double)track->x;
    return (uint32_t)x;
}

static p19_status one_bar(fb_target *fb, p19_axis axis, double degrees,
                          uint32_t colour)
{
    p19_rect t;
    p19_status s = p19_bar_track(&fb->geo, axis, &t);
    if (s != P19_OK)
        return s;

    /* The track, then the centre mark, then the marker, in that order so
     * that the last thing drawn is the thing being read. */
    fb_fill(fb, t.x, t.y, t.w, t.h, P19_COLOUR_TRACK);

    const uint32_t mark_w = (t.w / 100u) + 1u;
    fb_fill(fb, t.x + t.w / 2u - mark_w / 2u, t.y, mark_w, t.h,
            P19_COLOUR_CENTRE);

    const uint32_t marker_w = (t.w / 40u) + 2u;
    uint32_t x = p19_marker_x(&t, degrees, P19_FULL_SCALE_DEG);
    if (x > marker_w / 2u)
        x -= marker_w / 2u;
    fb_fill(fb, x, t.y, marker_w, t.h, colour);
    return P19_OK;
}

p19_status p19_paint(fb_target *fb, const p19_attitude *a)
{
    if (fb == NULL || fb->map == NULL || a == NULL)
        return P19_E_ARG;

    fb_fill(fb, 0, 0, fb->geo.width, fb->geo.height, P19_COLOUR_BACKGROUND);
    p19_status s = one_bar(fb, P19_AXIS_PITCH, a->pitch_deg, P19_COLOUR_PITCH);
    if (s != P19_OK)
        return s;
    return one_bar(fb, P19_AXIS_ROLL, a->roll_deg, P19_COLOUR_ROLL);
}
