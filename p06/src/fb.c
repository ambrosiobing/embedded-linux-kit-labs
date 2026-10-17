/* What P06 draws, which is an 8x8 grid of ranging zones.
 *
 * The framebuffer itself moved to lib/framebuffer.c when P19 needed to draw
 * on the same panel. What is left here is the part that knows what it is
 * painting, and the three forwards that keep this lab's own names: the
 * chapter, the suite and six call sites use them, and renaming those would
 * change what a reader sees without changing anything they can observe.
 */

#include "p06.h"

static p06_status from_fb(fb_status s)
{
    switch (s) {
    case FB_OK:       return P06_OK;
    case FB_E_IO:     return P06_E_IO;
    case FB_E_FORMAT: return P06_E_FORMAT;
    case FB_E_RANGE:  return P06_E_RANGE;
    case FB_E_ARG:    return P06_E_ARG;
    }
    return P06_E_IO;
}

p06_status p06_geometry_parse(p06_geometry *g, const char *spec)
{
    return from_fb(fb_geometry_parse(g, spec));
}

p06_status p06_geometry_from_sysfs(p06_geometry *g, const char *dir)
{
    return from_fb(fb_geometry_from_sysfs(g, dir));
}

void p06_fb_init(p06_fb *fb)
{
    fb_init(fb);
}

p06_status p06_fb_open(p06_fb *fb, const char *path, const p06_geometry *g)
{
    return from_fb(fb_open(fb, path, g));
}

p06_status p06_fb_close(p06_fb *fb)
{
    return from_fb(fb_close(fb));
}

p06_status p06_fb_fill(p06_fb *fb, uint32_t x, uint32_t y,
                       uint32_t w, uint32_t h, uint32_t rgb)
{
    return from_fb(fb_fill(fb, x, y, w, h, rgb));
}

uint32_t p06_zone_colour(uint16_t mm, const p06_book *b)
{
    /* Five steps and no gradient. A heat map with 256 shades invites reading
     * a distance off the glass, and this sensor's business is which zones are
     * occupied, not how far away the wall is to the millimetre. */
    static const uint32_t FREE[4] = {
        0x002040u,   /* furthest */
        0x004080u,
        0x0080C0u,
        0x00C0FFu    /* nearest, still beyond the threshold */
    };
    const uint32_t NO_TARGET = 0x101010u;
    const uint32_t OCCUPIED  = 0xFF3000u;

    if (mm == 0)
        return NO_TARGET;
    if (b == NULL || b->threshold_mm == 0)
        return FREE[1];
    if (mm < b->threshold_mm)
        return OCCUPIED;

    uint32_t span = b->nearest_wall_mm > b->threshold_mm
                  ? (uint32_t)(b->nearest_wall_mm - b->threshold_mm) : 1u;
    uint32_t over = (uint32_t)mm - b->threshold_mm;
    uint32_t step = (over * 4u) / (span + 1u);
    if (step > 3u)
        step = 3u;
    return FREE[3u - step];
}

p06_status p06_fb_paint(fb_target *fb, const p06_frame *f, const p06_book *b)
{
    if (fb == NULL || fb->map == NULL || f == NULL)
        return P06_E_ARG;

    const uint32_t cell_w = fb->geo.width / P06_SIDE;
    const uint32_t cell_h = fb->geo.height / P06_SIDE;
    if (cell_w == 0 || cell_h == 0)
        return P06_E_RANGE;   /* a display too small for one pixel per zone */

    /* Zone 0 at the top left, running row by row. This is a convention this
     * program fixes, not something read off the sensor, and the bench
     * confirms it by putting a hand in a known corner. */
    for (uint16_t row = 0; row < P06_SIDE; row++) {
        for (uint16_t col = 0; col < P06_SIDE; col++) {
            uint16_t mm = f->mm[row * P06_SIDE + col];
            uint32_t rgb = p06_zone_colour(mm, b);
            p06_status s = from_fb(fb_fill(fb, col * cell_w, row * cell_h,
                                           cell_w, cell_h, rgb));
            if (s != P06_OK)
                return s;
        }
    }
    return P06_OK;
}
