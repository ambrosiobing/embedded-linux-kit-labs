/* The device half: three lines on a GPIO character device.
 *
 * This file talks to the kernel's GPIO v2 uAPI directly, the way lib/i2cbus.h
 * talks to i2c-dev. The chapter's own commands use libgpiod, and that is
 * right for a command line; a program in this volume takes the ioctls
 * instead, for one reason that is about the book rather than about the code.
 * Every suite here has to build on a machine with no bench attached to it,
 * and linux/gpio.h is part of the kernel headers that are already present
 * wherever these programs are compiled. A lab whose program only builds on
 * the board it is waiting for is a lab that cannot be checked while it
 * waits, and nine of the labs in this volume are waiting.
 *
 * Nothing here prints. The report is filled and handed back, and main.c
 * decides what a reader sees, so this file has no opinion about which
 * failures deserve a paragraph.
 *
 * There is no signal handler in this program, deliberately. The lines are
 * released when line_fd closes, and the kernel closes it when the process
 * ends however it ends, including under a signal that no handler sees. That
 * is the whole of the chapter's "restore the pins on the way out", and it is
 * stronger than a handler because there is no path through it that can fail.
 */
/* O_CLOEXEC is POSIX.1-2008 and -std=c11 hides it, which is the same trap
 * that hides M_PI in P19. This is the line lib/framebuffer.c, p01 and p19
 * already declare for the same reason, so it is the volume's answer rather
 * than a second one.
 *
 * It is also enough on its own. glibc declares ioctl in sys/ioctl.h with no
 * feature guard, which is why lib/i2cbus.h compiles in P01 and P19 under
 * exactly this macro and nothing more, so there is no reason to reach for
 * _DEFAULT_SOURCE as lib/atchat.c has to for its three termios names.
 */
#define _POSIX_C_SOURCE 200809L

#include "p15.h"

#include <string.h>

#if defined(__linux__)
#include <linux/gpio.h>
#endif

#if defined(__linux__) && defined(GPIO_V2_GET_LINE_IOCTL)

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define P15_CONSUMER "p15-semaphore"

/* The kernel's fixed-width name fields are NUL terminated in practice, and a
 * bounded copy costs nothing and does not depend on that. strncpy is avoided
 * here because the obvious call draws a truncation warning under this
 * volume's gate, and silencing a warning is not the same as being right. */
static void copy_fixed(char *dst, size_t dstsz, const char *src, size_t srcsz)
{
    size_t n = 0;
    while (n + 1u < dstsz && n < srcsz && src[n] != '\0') {
        dst[n] = src[n];
        n++;
    }
    dst[n] = '\0';
}

static int chip_open(const char *chip_path)
{
    const char *path = (chip_path != NULL) ? chip_path : P15_CHIP_PATH;
    return open(path, O_RDWR | O_CLOEXEC);
}

p15_status p15_probe(const char *chip_path, p15_report *out)
{
    if (out == NULL)
        return P15_E_ARG;
    memset(out, 0, sizeof *out);

    int fd = chip_open(chip_path);
    if (fd < 0)
        return P15_E_NO_CHIP;

    struct gpiochip_info info;
    memset(&info, 0, sizeof info);
    if (ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info) < 0) {
        close(fd);
        return P15_E_CHIP_INFO;
    }
    copy_fixed(out->chip_label, sizeof out->chip_label,
               info.label, sizeof info.label);
    out->chip_lines = info.lines;

    /* Reported, not judged. A probe that refuses a chip tells a reader
     * nothing about what is on it, and what is on it is the question. */
    for (unsigned ch = 0; ch < P15_CHANNELS; ch++) {
        const p15_wire *w = p15_wire_of(ch);
        if (w == NULL || w->offset >= info.lines)
            continue;

        struct gpio_v2_line_info li;
        memset(&li, 0, sizeof li);
        li.offset = w->offset;
        if (ioctl(fd, GPIO_V2_GET_LINEINFO_IOCTL, &li) < 0)
            continue;

        out->used[ch] = (li.flags & GPIO_V2_LINE_FLAG_USED) != 0;
        copy_fixed(out->consumer[ch], sizeof out->consumer[ch],
                   li.consumer, sizeof li.consumer);
    }

    close(fd);
    return P15_OK;
}

p15_status p15_lines_open(p15_lines *l, const char *chip_path, bool any_chip,
                          p15_polarity polarity)
{
    if (l == NULL)
        return P15_E_ARG;
    l->chip_fd = -1;
    l->line_fd = -1;
    l->polarity = polarity;
    l->last_bits = 0;

    int fd = chip_open(chip_path);
    if (fd < 0)
        return P15_E_NO_CHIP;

    struct gpiochip_info info;
    memset(&info, 0, sizeof info);
    if (ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info) < 0) {
        close(fd);
        return P15_E_CHIP_INFO;
    }

    char label[32];
    copy_fixed(label, sizeof label, info.label, sizeof info.label);
    if (!any_chip && !p15_chip_matches(label)) {
        close(fd);
        return P15_E_WRONG_CHIP;
    }

    /* An offset past the end of this chip is the same conclusion by a
     * different route: the wiring table is not from this chip. */
    for (unsigned ch = 0; ch < P15_CHANNELS; ch++) {
        const p15_wire *w = p15_wire_of(ch);
        if (w == NULL || w->offset >= info.lines) {
            close(fd);
            return P15_E_WRONG_CHIP;
        }
    }

    struct gpio_v2_line_request req;
    memset(&req, 0, sizeof req);
    for (unsigned ch = 0; ch < P15_CHANNELS; ch++)
        req.offsets[ch] = p15_wire_of(ch)->offset;
    req.num_lines = P15_CHANNELS;
    copy_fixed(req.consumer, sizeof req.consumer,
               P15_CONSUMER, sizeof P15_CONSUMER);

    /* The initial value is part of the request rather than a write that
     * follows it, so the lines go from released straight to dark and are
     * never briefly driven to a state nobody chose.
     *
     * One alignment to keep in mind through the rest of this file: every
     * mask and every bits field below is indexed by a line's position in
     * req.offsets, not by its offset on the chip. The channel order in
     * p15_wiring() is the order used here, so a channel's bit in a lit set
     * is that same line's bit in the kernel's values, and no translation
     * happens anywhere. The suite pins that order so it stays true. */
    uint8_t dark = p15_dark_bits(polarity);
    req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
    req.config.num_attrs = 1;
    req.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
    req.config.attrs[0].attr.values = dark;
    req.config.attrs[0].mask = P15_LIT_ALL;

    if (ioctl(fd, GPIO_V2_GET_LINE_IOCTL, &req) < 0) {
        int err = errno;
        close(fd);
        return (err == EBUSY) ? P15_E_BUSY : P15_E_REQUEST;
    }

    l->chip_fd = fd;
    l->line_fd = req.fd;
    l->last_bits = dark;
    return P15_OK;
}

p15_status p15_lines_set(p15_lines *l, uint8_t lit)
{
    if (l == NULL || l->line_fd < 0)
        return P15_E_ARG;

    struct gpio_v2_line_values vals;
    memset(&vals, 0, sizeof vals);
    vals.bits = p15_bits_for(lit, l->polarity);
    vals.mask = P15_LIT_ALL;

    if (ioctl(l->line_fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &vals) < 0)
        return P15_E_SET;

    l->last_bits = (uint8_t)vals.bits;
    return P15_OK;
}

void p15_lines_close(p15_lines *l)
{
    if (l == NULL)
        return;
    if (l->line_fd >= 0)
        close(l->line_fd);
    if (l->chip_fd >= 0)
        close(l->chip_fd);
    l->line_fd = -1;
    l->chip_fd = -1;
}

#else /* not Linux, or kernel headers without the v2 uAPI */

/* The argument checks are the same on both sides of this #if, because a
 * caller who hands over nowhere to put the answer has made a mistake whether
 * or not there is a kernel underneath. Only the last line of each function
 * belongs to the platform. */

p15_status p15_probe(const char *chip_path, p15_report *out)
{
    (void)chip_path;
    if (out == NULL)
        return P15_E_ARG;
    memset(out, 0, sizeof *out);
    return P15_E_PLATFORM;
}

p15_status p15_lines_open(p15_lines *l, const char *chip_path, bool any_chip,
                          p15_polarity polarity)
{
    (void)chip_path;
    (void)any_chip;
    if (l == NULL)
        return P15_E_ARG;
    l->chip_fd = -1;
    l->line_fd = -1;
    l->polarity = polarity;
    l->last_bits = 0;
    return P15_E_PLATFORM;
}

p15_status p15_lines_set(p15_lines *l, uint8_t lit)
{
    (void)lit;
    if (l == NULL || l->line_fd < 0)
        return P15_E_ARG;
    return P15_E_PLATFORM;
}

void p15_lines_close(p15_lines *l)
{
    (void)l;
}

#endif

const char *p15_strstatus(p15_status s)
{
    switch (s) {
    case P15_OK:
        return "ok";
    case P15_E_ARG:
        return "a caller's mistake, which is a defect in this program";
    case P15_E_NO_CHIP:
        return "the chip would not open: wrong path, or this user is not in "
               "the gpio group";
    case P15_E_CHIP_INFO:
        return "the chip opened and would not say what it is";
    case P15_E_WRONG_CHIP:
        return "this is not the chip the wiring table was recorded against";
    case P15_E_BUSY:
        return "another process holds one of these lines";
    case P15_E_REQUEST:
        return "the line request failed";
    case P15_E_SET:
        return "the values did not reach the lines";
    case P15_E_PLATFORM:
        return "built without the Linux GPIO character device, so there is "
               "nothing here to drive";
    }
    return "an unnamed status, which should not be reachable";
}
