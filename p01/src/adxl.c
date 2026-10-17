/* The bus, and two forwards to the part.
 *
 * The decode moved to lib/adxl345.h when P19 turned out to read the same six
 * bytes, and the reasoning that goes with it moved there too: the registers
 * and the two's complement are properties of the device rather than of any
 * lab. What is left here is this lab's names and the i2c-dev sequence that
 * reaches the device at all.
 */
#define _POSIX_C_SOURCE 200809L

#include "p01.h"

#include <fcntl.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#endif

const char *p01_strerror(p01_status s)
{
    switch (s) {
    case P01_OK:       return "ok";
    case P01_E_IO:     return "the bus or the file could not be reached";
    case P01_E_FORMAT: return "the content is not in the expected form";
    case P01_E_ARG:    return "an argument is missing or will not do";
    }
    return "unknown";
}

/* Both of these are lib/adxl345.h now. They stay as functions here so that
 * this lab's own names survive in a stack trace and in its suite. */

void p01_decode(const uint8_t raw[6], p01_sample *out)
{
    adxl345_decode(raw, out);
}

bool p01_parse_bytes(const char *text, uint8_t raw[6])
{
    return adxl345_parse_bytes(text, raw);
}

/* ---------------------------------------------------------------- the bus */

#if defined(__linux__)

p01_status p01_device_open(int *fd, int bus, uint8_t address)
{
    if (fd == NULL || bus < 0)
        return P01_E_ARG;
    *fd = -1;

    char path[32];
    int n = snprintf(path, sizeof path, "/dev/i2c-%d", bus);
    if (n < 0 || (size_t)n >= sizeof path)
        return P01_E_ARG;

    int f = open(path, O_RDWR);
    if (f < 0)
        return P01_E_IO;
    if (ioctl(f, I2C_SLAVE, (unsigned long)address) < 0) {
        close(f);
        return P01_E_IO;
    }
    *fd = f;
    return P01_OK;
}

static p01_status write_reg(int fd, uint8_t reg, uint8_t value)
{
    uint8_t pair[2] = { reg, value };
    return (write(fd, pair, 2) == 2) ? P01_OK : P01_E_IO;
}

p01_status p01_device_configure(int fd)
{
    if (fd < 0)
        return P01_E_ARG;
    /* Measure, then full resolution. Order matters less than the fact that
     * both are set: a device left in its default range reports a tenth of
     * the acceleration it should and every threshold in the chapter is then
     * wrong by the same factor. */
    p01_status s = write_reg(fd, P01_REG_POWER_CTL, P01_POWER_MEASURE);
    if (s != P01_OK)
        return s;
    return write_reg(fd, P01_REG_DATA_FORMAT, P01_FORMAT_FULL_RES_16G);
}

p01_status p01_device_read(int fd, uint8_t raw[6])
{
    if (fd < 0 || raw == NULL)
        return P01_E_ARG;
    uint8_t reg = P01_REG_DATAX0;
    if (write(fd, &reg, 1) != 1)
        return P01_E_IO;
    if (read(fd, raw, 6) != 6)
        return P01_E_IO;
    return P01_OK;
}

void p01_device_close(int fd)
{
    if (fd >= 0)
        close(fd);
}

#else

/* Not Linux. The decode, the policy and the budget are all still here and
 * all still testable; only the bus is absent, and saying so beats a stub
 * that returns plausible numbers. */
p01_status p01_device_open(int *fd, int bus, uint8_t address)
{
    (void)bus;
    (void)address;
    if (fd != NULL)
        *fd = -1;
    return P01_E_IO;
}

p01_status p01_device_configure(int fd)
{
    (void)fd;
    return P01_E_IO;
}

p01_status p01_device_read(int fd, uint8_t raw[6])
{
    (void)fd;
    (void)raw;
    return P01_E_IO;
}

void p01_device_close(int fd)
{
    (void)fd;
}

#endif
