/* The bus, and two forwards to the part.
 *
 * The decode moved to lib/adxl345.h when P19 turned out to read the same six
 * bytes, and the reasoning that goes with it moved there too: the registers
 * and the two's complement are properties of the device rather than of any
 * lab. What is left here is this lab's names and the i2c-dev sequence that
 * reaches the device at all.
 */
#include "p01.h"

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

/* lib/i2cbus.h now, with P19, which reads the same device over the same
 * four steps. These keep this lab's names and its status type. */

p01_status p01_device_open(int *fd, int bus, uint8_t address)
{
    return i2cbus_open(fd, bus, address) ? P01_OK : P01_E_IO;
}

p01_status p01_device_configure(int fd)
{
    /* Measure, then full resolution. A device left in its default range
     * reports a tenth of the acceleration it should, and every threshold in
     * the chapter is then wrong by the same factor. */
    if (!i2cbus_write_reg(fd, P01_REG_POWER_CTL, P01_POWER_MEASURE))
        return P01_E_IO;
    if (!i2cbus_write_reg(fd, P01_REG_DATA_FORMAT, P01_FORMAT_FULL_RES_16G))
        return P01_E_IO;
    return P01_OK;
}

p01_status p01_device_read(int fd, uint8_t raw[6])
{
    return i2cbus_read_reg(fd, P01_REG_DATAX0, raw, 6) ? P01_OK : P01_E_IO;
}

void p01_device_close(int fd)
{
    i2cbus_close(fd);
}
