/* The framebuffer. See framebuffer.h for why the geometry is read from the
 * two files the kernel publishes rather than through an ioctl.
 */
#define _POSIX_C_SOURCE 200809L

#include "framebuffer.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

const char *fb_strerror(fb_status s)
{
    switch (s) {
    case FB_OK:       return "ok";
    case FB_E_IO:     return "the display could not be opened or mapped";
    case FB_E_FORMAT: return "the geometry is not in the expected form";
    case FB_E_RANGE:  return "a value is outside the range this code allows";
    case FB_E_ARG:    return "an argument is missing or will not do";
    }
    return "unknown";
}

fb_status fb_geometry_parse(fb_geometry *g, const char *spec)
{
    if (g == NULL || spec == NULL)
        return FB_E_ARG;
    unsigned w = 0, h = 0, bpp = 0;
    if (sscanf(spec, "%ux%ux%u", &w, &h, &bpp) != 3)
        return FB_E_FORMAT;
    if (w == 0 || h == 0)
        return FB_E_RANGE;
    if (bpp != 16 && bpp != 32)
        return FB_E_RANGE;
    g->width = w;
    g->height = h;
    g->bpp = bpp;
    return FB_OK;
}

static fb_status read_small_file(const char *dir, const char *name,
                                  char *out, size_t cap)
{
    char path[512];
    int n = snprintf(path, sizeof path, "%s/%s", dir, name);
    if (n < 0 || (size_t)n >= sizeof path)
        return FB_E_ARG;
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return FB_E_IO;
    if (fgets(out, (int)cap, fp) == NULL) {
        fclose(fp);
        return FB_E_FORMAT;
    }
    fclose(fp);
    return FB_OK;
}

fb_status fb_geometry_from_sysfs(fb_geometry *g, const char *dir)
{
    if (g == NULL || dir == NULL)
        return FB_E_ARG;

    char size[64];
    char depth[64];
    fb_status s = read_small_file(dir, "virtual_size", size, sizeof size);
    if (s != FB_OK)
        return s;
    s = read_small_file(dir, "bits_per_pixel", depth, sizeof depth);
    if (s != FB_OK)
        return s;

    unsigned w = 0, h = 0;
    if (sscanf(size, "%u,%u", &w, &h) != 2)
        return FB_E_FORMAT;
    unsigned bpp = (unsigned)atoi(depth);

    if (w == 0 || h == 0)
        return FB_E_RANGE;
    if (bpp != 16 && bpp != 32)
        return FB_E_RANGE;

    g->width = w;
    g->height = h;
    g->bpp = bpp;
    return FB_OK;
}

void fb_init(fb_target *fb)
{
    if (fb == NULL)
        return;
    memset(fb, 0, sizeof *fb);
    fb->fd = -1;
}

fb_status fb_open(fb_target *fb, const char *path, const fb_geometry *g)
{
    if (fb == NULL || path == NULL || g == NULL)
        return FB_E_ARG;
    if (g->bpp != 16 && g->bpp != 32)
        return FB_E_RANGE;
    if (g->width == 0 || g->height == 0)
        return FB_E_RANGE;

    fb_init(fb);
    fb->geo = *g;
    fb->stride = (size_t)g->width * (g->bpp / 8u);
    fb->bytes = fb->stride * (size_t)g->height;

    fb->fd = open(path, O_RDWR);
    if (fb->fd < 0)
        return FB_E_IO;

    /* A regular file shorter than the geometry would be mapped anyway, and
     * writing into the part past the end of the file raises a signal rather
     * than an error. Refuse it here, where it can still be said in words. */
    struct stat st;
    if (fstat(fb->fd, &st) == 0 && S_ISREG(st.st_mode)) {
        if ((size_t)st.st_size < fb->bytes) {
            close(fb->fd);
            fb->fd = -1;
            return FB_E_RANGE;
        }
    }

    void *m = mmap(NULL, fb->bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, 0);
    if (m == MAP_FAILED) {
        close(fb->fd);
        fb->fd = -1;
        return FB_E_IO;
    }
    fb->map = (unsigned char *)m;
    return FB_OK;
}

fb_status fb_close(fb_target *fb)
{
    if (fb == NULL)
        return FB_E_ARG;
    fb_status s = FB_OK;
    if (fb->map != NULL && munmap(fb->map, fb->bytes) != 0)
        s = FB_E_IO;
    if (fb->fd >= 0 && close(fb->fd) != 0)
        s = FB_E_IO;
    fb->map = NULL;
    fb->fd = -1;
    return s;
}

static void put_pixel(fb_target *fb, uint32_t x, uint32_t y, uint32_t rgb)
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

fb_status fb_fill(fb_target *fb, uint32_t x, uint32_t y,
                       uint32_t w, uint32_t h, uint32_t rgb)
{
    if (fb == NULL || fb->map == NULL)
        return FB_E_ARG;

    /* Clipped, and a rectangle entirely outside is not an error. The grid
     * arithmetic above is allowed to be simple only because this is not. */
    if (x >= fb->geo.width || y >= fb->geo.height)
        return FB_OK;
    if (w > fb->geo.width - x)
        w = fb->geo.width - x;
    if (h > fb->geo.height - y)
        h = fb->geo.height - y;

    for (uint32_t row = 0; row < h; row++)
        for (uint32_t col = 0; col < w; col++)
            put_pixel(fb, x + col, y + row, rgb);
    return FB_OK;
}
