/* An I2C device on Linux, through i2c-dev, for the labs that read one.
 *
 * Open the bus, tell the driver which address you are talking to, write a
 * register number, read the bytes. That is the whole of it, and it is the
 * same four steps in P01, P09 and P19, so it lives here rather than in
 * whichever lab happened to need it first.
 *
 * Everything returns a bool and nothing allocates. A false here means the
 * bus or the device did not answer, which on a first run is the ordinary
 * answer and is why every chapter in this volume says to run i2cdetect
 * before anything else: an absent device and an absent bus look identical
 * from inside a program, and only a bench can tell them apart.
 *
 * Header only, with static inline functions, so no Makefile has to learn
 * about it. On anything that is not Linux every call fails, by saying so
 * rather than by returning plausible numbers from a stub.
 */
#ifndef I2CBUS_H
#define I2CBUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if defined(__linux__)

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>

static inline bool i2cbus_open(int *fd, int bus, uint8_t address)
{
    if (fd == NULL || bus < 0)
        return false;
    *fd = -1;

    char path[32];
    int n = snprintf(path, sizeof path, "/dev/i2c-%d", bus);
    if (n < 0 || (size_t)n >= sizeof path)
        return false;

    int f = open(path, O_RDWR);
    if (f < 0)
        return false;
    if (ioctl(f, I2C_SLAVE, (unsigned long)address) < 0) {
        close(f);
        return false;
    }
    *fd = f;
    return true;
}

static inline bool i2cbus_write_reg(int fd, uint8_t reg, uint8_t value)
{
    if (fd < 0)
        return false;
    uint8_t pair[2] = { reg, value };
    return write(fd, pair, 2) == 2;
}

/* The register pointer, then the read. Two transfers rather than one
 * combined transaction, which is what every i2c-dev example does and what
 * every device in this kit accepts. */
static inline bool i2cbus_read_reg(int fd, uint8_t reg, uint8_t *out, size_t n)
{
    if (fd < 0 || out == NULL || n == 0)
        return false;
    if (write(fd, &reg, 1) != 1)
        return false;
    return read(fd, out, n) == (ssize_t)n;
}

static inline void i2cbus_close(int fd)
{
    if (fd >= 0)
        close(fd);
}

#else

static inline bool i2cbus_open(int *fd, int bus, uint8_t address)
{
    (void)bus;
    (void)address;
    if (fd != NULL)
        *fd = -1;
    return false;
}

static inline bool i2cbus_write_reg(int fd, uint8_t reg, uint8_t value)
{
    (void)fd;
    (void)reg;
    (void)value;
    return false;
}

static inline bool i2cbus_read_reg(int fd, uint8_t reg, uint8_t *out, size_t n)
{
    (void)fd;
    (void)reg;
    (void)out;
    (void)n;
    return false;
}

static inline void i2cbus_close(int fd)
{
    (void)fd;
}

#endif

#endif /* I2CBUS_H */
