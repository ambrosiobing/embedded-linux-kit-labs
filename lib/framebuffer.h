/* A framebuffer, for the labs that draw on one.
 *
 * A framebuffer is a mapping of memory whose shape is decided by someone
 * else, which is the whole hazard. So the geometry is read rather than
 * assumed, every rectangle is clipped to it, and nothing here writes a byte
 * past the length that geometry implies.
 *
 * The geometry comes from the two text files the kernel already publishes,
 *
 *   /sys/class/graphics/fb0/virtual_size      "480,320"
 *   /sys/class/graphics/fb0/bits_per_pixel    "16"
 *
 * rather than from an ioctl. Three reasons, in the order they matter. A test
 * can point this at a directory it wrote itself, so the clipping is testable
 * with no display anywhere. It needs no Linux-only header, so this is
 * ordinary C that happens to run on Linux. And a reader at the bench can run
 * cat on the same two files and see the numbers the program is using, which
 * an ioctl does not allow.
 *
 * Sixteen and thirty-two bits per pixel are painted. Anything else is
 * refused by name: a display this code does not understand gets no pattern
 * written to it on the chance that it might look right.
 *
 * This was P06's, and moved here when P19 needed to draw on the same panel.
 * What stayed behind in each lab is the part that knows what it is drawing:
 * P06 paints an 8x8 grid of ranging zones and P19 paints two bars, and
 * neither of those belongs to a framebuffer.
 */
#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    FB_OK = 0,
    FB_E_IO,
    FB_E_FORMAT,
    FB_E_RANGE,
    FB_E_ARG
} fb_status;

const char *fb_strerror(fb_status s);

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;      /* 16 or 32. Anything else is refused by name */
} fb_geometry;

fb_status fb_geometry_from_sysfs(fb_geometry *g, const char *dir);
/* From a string of the form 480x320x16, for a bench with neither. */
fb_status fb_geometry_parse(fb_geometry *g, const char *spec);

typedef struct {
    int fd;
    unsigned char *map;
    size_t bytes;      /* what the geometry says, which is what may be written */
    size_t stride;
    fb_geometry geo;
} fb_target;

void fb_init(fb_target *fb);
/* Maps exactly width * height * bpp / 8 bytes and refuses to open a regular
 * file shorter than that. Nothing here ever writes past that length. */
fb_status fb_open(fb_target *fb, const char *path, const fb_geometry *g);
fb_status fb_close(fb_target *fb);
/* Clipped to the geometry. A rectangle that starts outside it paints nothing
 * and is not an error, because the arithmetic in the callers is allowed to
 * be simple only if this is not. */
fb_status fb_fill(fb_target *fb, uint32_t x, uint32_t y,
                  uint32_t w, uint32_t h, uint32_t rgb);

#endif /* FRAMEBUFFER_H */
