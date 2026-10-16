/* The glass.
 *
 * A framebuffer is a mapping of memory whose shape is decided by someone
 * else, which is the whole hazard: the geometry is read rather than assumed,
 * every rectangle is clipped to it, and nothing here writes a byte past the
 * length that geometry implies.
 *
 * The geometry comes from the two text files the kernel already publishes,
 *
 *   /sys/class/graphics/fb0/virtual_size      "480,320"
 *   /sys/class/graphics/fb0/bits_per_pixel    "16"
 *
 * rather than from an ioctl. Three reasons, in order of how much they
 * matter. A test can point this at a directory it wrote itself, so the
 * clipping is testable with no display anywhere. It needs no Linux-only
 * header, so the program is ordinary C that happens to run on Linux. And a
 * reader at the bench can run cat on the same two files and see the numbers
 * the program is using, which an ioctl does not allow.
 *
 * Sixteen and thirty-two bits per pixel are painted. Anything else is
 * refused by name. A display this program does not understand gets no
 * pattern written to it on the chance that it might look right.
 */
#define _POSIX_C_SOURCE 200809L

#include "p06.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

p06_status p06_geometry_parse(p06_geometry *g, const char *spec)
{
    if (g == NULL || spec == NULL)
        return P06_E_ARG;
    unsigned w = 0, h = 0, bpp = 0;
    if (sscanf(spec, "%ux%ux%u", &w, &h, &bpp) != 3)
        return P06_E_FORMAT;
    if (w == 0 || h == 0)
        return P06_E_RANGE;
    if (bpp != 16 && bpp != 32)
        return P06_E_RANGE;
    g->width = w;
    g->height = h;
    g->bpp = bpp;
    return P06_OK;
}

static p06_status read_small_file(const char *dir, const char *name,
                                  char *out, size_t cap)
{
    char path[512];
    int n = snprintf(path, sizeof path, "%s/%s", dir, name);
    if (n < 0 || (size_t)n >= sizeof path)
        return P06_E_ARG;
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return P06_E_IO;
    if (fgets(out, (int)cap, fp) == NULL) {
        fclose(fp);
        return P06_E_FORMAT;
    }
    fclose(fp);
    return P06_OK;
}

p06_status p06_geometry_from_sysfs(p06_geometry *g, const char *dir)
{
    if (g == NULL || dir == NULL)
        return P06_E_ARG;

    char size[64];
    char depth[64];
    p06_status s = read_small_file(dir, "virtual_size", size, sizeof size);
    if (s != P06_OK)
        return s;
    s = read_small_file(dir, "bits_per_pixel", depth, sizeof depth);
    if (s != P06_OK)
        return s;

    unsigned w = 0, h = 0;
    if (sscanf(size, "%u,%u", &w, &h) != 2)
        return P06_E_FORMAT;
    unsigned bpp = (unsigned)atoi(depth);

    if (w == 0 || h == 0)
        return P06_E_RANGE;
    if (bpp != 16 && bpp != 32)
        return P06_E_RANGE;

    g->width = w;
    g->height = h;
    g->bpp = bpp;
    return P06_OK;
}

void p06_fb_init(p06_fb *fb)
{
    if (fb == NULL)
        return;
    memset(fb, 0, sizeof *fb);
    fb->fd = -1;
}

p06_status p06_fb_open(p06_fb *fb, const char *path, const p06_geometry *g)
{
    if (fb == NULL || path == NULL || g == NULL)
        return P06_E_ARG;
    if (g->bpp != 16 && g->bpp != 32)
        return P06_E_RANGE;
    if (g->width == 0 || g->height == 0)
        return P06_E_RANGE;

    p06_fb_init(fb);
    fb->geo = *g;
    fb->stride = (size_t)g->width * (g->bpp / 8u);
    fb->bytes = fb->stride * (size_t)g->height;

    fb->fd = open(path, O_RDWR);
    if (fb->fd < 0)
        return P06_E_IO;

    /* A regular file shorter than the geometry would be mapped anyway, and
     * writing into the part past the end of the file raises a signal rather
     * than an error. Refuse it here, where it can still be said in words. */
    struct stat st;
    if (fstat(fb->fd, &st) == 0 && S_ISREG(st.st_mode)) {
        if ((size_t)st.st_size < fb->bytes) {
            close(fb->fd);
            fb->fd = -1;
            return P06_E_RANGE;
        }
    }

    void *m = mmap(NULL, fb->bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, 0);
    if (m == MAP_FAILED) {
        close(fb->fd);
        fb->fd = -1;
        return P06_E_IO;
    }
    fb->map = (unsigned char *)m;
    return P06_OK;
}

p06_status p06_fb_close(p06_fb *fb)
{
    if (fb == NULL)
        return P06_E_ARG;
    p06_status s = P06_OK;
    if (fb->map != NULL && munmap(fb->map, fb->bytes) != 0)
        s = P06_E_IO;
    if (fb->fd >= 0 && close(fb->fd) != 0)
        s = P06_E_IO;
    fb->map = NULL;
    fb->fd = -1;
    return s;
}

static void put_pixel(p06_fb *fb, uint32_t x, uint32_t y, uint32_t rgb)
{
    unsigned char *p = fb->map + (size_t)y * fb->stride + (size_t)x * (fb->geo.bpp / 8u);
    if (fb->geo.bpp == 16) {
        /* RGB565, little endian, which is what every panel in this kit
         * presents at 16 bits. */
        uint16_t v = (uint16_t)((((rgb >> 19) & 0x1Fu) << 11) |
                                (((rgb >> 10) & 0x3Fu) << 5) |
                                ((rgb >> 3) & 0x1Fu));
        p[0] = (unsigned char)(v & 0xFFu);
        p[1] = (unsigned char)(v >> 8);
    } else {
        p[0] = (unsigned char)(rgb & 0xFFu);          /* blue */
        p[1] = (unsigned char)((rgb >> 8) & 0xFFu);   /* green */
        p[2] = (unsigned char)((rgb >> 16) & 0xFFu);  /* red */
        p[3] = 0;
    }
}

p06_status p06_fb_fill(p06_fb *fb, uint32_t x, uint32_t y,
                       uint32_t w, uint32_t h, uint32_t rgb)
{
    if (fb == NULL || fb->map == NULL)
        return P06_E_ARG;

    /* Clipped, and a rectangle entirely outside is not an error. The grid
     * arithmetic above is allowed to be simple only because this is not. */
    if (x >= fb->geo.width || y >= fb->geo.height)
        return P06_OK;
    if (w > fb->geo.width - x)
        w = fb->geo.width - x;
    if (h > fb->geo.height - y)
        h = fb->geo.height - y;

    for (uint32_t row = 0; row < h; row++)
        for (uint32_t col = 0; col < w; col++)
            put_pixel(fb, x + col, y + row, rgb);
    return P06_OK;
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

p06_status p06_fb_paint(p06_fb *fb, const p06_frame *f, const p06_book *b)
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
            p06_status s = p06_fb_fill(fb, col * cell_w, row * cell_h,
                                       cell_w, cell_h, rgb);
            if (s != P06_OK)
                return s;
        }
    }
    return P06_OK;
}
