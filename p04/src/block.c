/* The capture file and the transport accounting.
 *
 * The file format is this program's own and is written down here rather than
 * inferred from the code that reads it:
 *
 *   offset  size  what
 *   0       4     magic, the bytes P 0 4 C
 *   4       2     version, little endian, currently 1
 *   6       2     block length in samples per channel
 *   8       4     block count as declared by the writer
 *   12      4     sample rate in hertz, IEEE 754 single, little endian
 *   16      4     g per count, IEEE 754 single, little endian
 *   20      ...   blocks
 *
 * and each block is
 *
 *   0       4     sequence number, little endian
 *   4       2L    vibration channel, int16 little endian, L samples
 *   4+2L    2L    microphone channel, same shape
 *
 * This is a capture format, not the probe's wire format. The chapter is
 * explicit that the probe's own framing comes from its firmware and must not
 * be invented, so nothing here claims to be it.
 *
 * Everything is written byte by byte rather than by dropping a struct on the
 * file, because a struct's padding and the host's endianness are both free to
 * change and a capture written on a laptop is meant to be read on a board.
 */

#include "p04.h"

#include <string.h>

#define P04_MAGIC0 'P'
#define P04_MAGIC1 '0'
#define P04_MAGIC2 '4'
#define P04_MAGIC3 'C'
#define P04_VERSION 1u
#define P04_HEADER_BYTES 20

const char *p04_strerror(p04_status s)
{
    switch (s) {
    case P04_OK:       return "ok";
    case P04_E_IO:     return "the file would not open, read or write";
    case P04_E_FORMAT: return "not a capture this program wrote";
    case P04_E_RANGE:  return "a value is outside what the fixed buffers allow";
    case P04_E_ARG:    return "an argument is impossible";
    case P04_E_EOF:    return "no more blocks";
    }
    return "unknown";
}

/* ---------------------------------------------------------------- bytes */

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* A float travels as its IEEE 754 bit pattern. memcpy rather than a cast
 * through a pointer, because the cast is undefined and the compiler is within
 * its rights to assume it never happens. */
static void put_f32(uint8_t *p, float v)
{
    uint32_t bits;
    memcpy(&bits, &v, sizeof bits);
    put_u32(p, bits);
}

static float get_f32(const uint8_t *p)
{
    uint32_t bits = get_u32(p);
    float v;
    memcpy(&v, &bits, sizeof v);
    return v;
}

/* ---------------------------------------------------------------- file */

p04_status p04_capture_create(p04_capture *c, const char *path, uint16_t block_len,
                              float rate_hz, float g_per_count)
{
    if (c == NULL || path == NULL)
        return P04_E_ARG;
    if (block_len == 0 || block_len > P04_BLOCK_MAX)
        return P04_E_RANGE;
    if (rate_hz <= 0.0f)
        return P04_E_ARG;

    memset(c, 0, sizeof *c);
    c->fp = fopen(path, "wb");
    if (c->fp == NULL)
        return P04_E_IO;
    c->writing = true;
    c->block_len = block_len;
    c->rate_hz = rate_hz;
    c->g_per_count = g_per_count;

    /* The block count is not known until the writer is closed, so a zero goes
     * in now and the real figure is written back over it at close. A reader
     * that sees zero and a non-empty file trusts what it can actually read. */
    uint8_t h[P04_HEADER_BYTES];
    h[0] = P04_MAGIC0; h[1] = P04_MAGIC1; h[2] = P04_MAGIC2; h[3] = P04_MAGIC3;
    put_u16(h + 4, P04_VERSION);
    put_u16(h + 6, block_len);
    put_u32(h + 8, 0u);
    put_f32(h + 12, rate_hz);
    put_f32(h + 16, g_per_count);
    if (fwrite(h, 1, sizeof h, c->fp) != sizeof h) {
        fclose(c->fp);
        c->fp = NULL;
        return P04_E_IO;
    }
    return P04_OK;
}

p04_status p04_capture_open(p04_capture *c, const char *path)
{
    if (c == NULL || path == NULL)
        return P04_E_ARG;
    memset(c, 0, sizeof *c);
    c->fp = fopen(path, "rb");
    if (c->fp == NULL)
        return P04_E_IO;

    uint8_t h[P04_HEADER_BYTES];
    if (fread(h, 1, sizeof h, c->fp) != sizeof h) {
        fclose(c->fp);
        c->fp = NULL;
        return P04_E_FORMAT;
    }
    if (h[0] != P04_MAGIC0 || h[1] != P04_MAGIC1 ||
        h[2] != P04_MAGIC2 || h[3] != P04_MAGIC3) {
        fclose(c->fp);
        c->fp = NULL;
        return P04_E_FORMAT;
    }
    if (get_u16(h + 4) != P04_VERSION) {
        fclose(c->fp);
        c->fp = NULL;
        return P04_E_FORMAT;
    }
    c->block_len = get_u16(h + 6);
    c->block_count = get_u32(h + 8);
    c->rate_hz = get_f32(h + 12);
    c->g_per_count = get_f32(h + 16);

    if (c->block_len == 0 || c->block_len > P04_BLOCK_MAX) {
        /* Refused at the header rather than part way through, so that a
         * capture which does not fit is a clean failure rather than a short
         * read in the middle of an analysis. */
        fclose(c->fp);
        c->fp = NULL;
        return P04_E_RANGE;
    }
    if (!(c->rate_hz > 0.0f)) {
        fclose(c->fp);
        c->fp = NULL;
        return P04_E_FORMAT;
    }
    c->writing = false;
    return P04_OK;
}

p04_status p04_capture_write(p04_capture *c, const p04_block *b)
{
    if (c == NULL || b == NULL || c->fp == NULL || !c->writing)
        return P04_E_ARG;
    if (b->len != c->block_len)
        return P04_E_RANGE;

    uint8_t seq[4];
    put_u32(seq, b->seq);
    if (fwrite(seq, 1, sizeof seq, c->fp) != sizeof seq)
        return P04_E_IO;

    for (int ch = 0; ch < 2; ch++) {
        const int16_t *src = (ch == 0) ? b->vib : b->mic;
        for (uint16_t i = 0; i < b->len; i++) {
            uint8_t s[2];
            put_u16(s, (uint16_t)src[i]);
            if (fwrite(s, 1, sizeof s, c->fp) != sizeof s)
                return P04_E_IO;
        }
    }
    c->blocks_done++;
    return P04_OK;
}

p04_status p04_capture_read(p04_capture *c, p04_block *b)
{
    if (c == NULL || b == NULL || c->fp == NULL || c->writing)
        return P04_E_ARG;

    uint8_t seq[4];
    size_t got = fread(seq, 1, sizeof seq, c->fp);
    if (got == 0)
        return P04_E_EOF;
    if (got != sizeof seq)
        return P04_E_FORMAT;

    b->seq = get_u32(seq);
    b->len = c->block_len;
    for (int ch = 0; ch < 2; ch++) {
        int16_t *dst = (ch == 0) ? b->vib : b->mic;
        for (uint16_t i = 0; i < b->len; i++) {
            uint8_t s[2];
            if (fread(s, 1, sizeof s, c->fp) != sizeof s)
                return P04_E_FORMAT;
            dst[i] = (int16_t)get_u16(s);
        }
    }
    c->blocks_done++;
    return P04_OK;
}

p04_status p04_capture_close(p04_capture *c)
{
    if (c == NULL)
        return P04_E_ARG;
    if (c->fp == NULL)
        return P04_OK;

    p04_status s = P04_OK;
    if (c->writing) {
        /* Write the true block count back over the placeholder. */
        if (fseek(c->fp, 8, SEEK_SET) != 0) {
            s = P04_E_IO;
        } else {
            uint8_t n[4];
            put_u32(n, c->blocks_done);
            if (fwrite(n, 1, sizeof n, c->fp) != sizeof n)
                s = P04_E_IO;
        }
    }
    if (fclose(c->fp) != 0 && s == P04_OK)
        s = P04_E_IO;
    c->fp = NULL;
    return s;
}

/* ---------------------------------------------------------------- transport */

void p04_transport_init(p04_transport *t)
{
    if (t != NULL)
        memset(t, 0, sizeof *t);
}

void p04_transport_see(p04_transport *t, uint32_t seq)
{
    if (t == NULL)
        return;
    if (!t->started) {
        t->started = true;
        t->first_seq = seq;
        t->last_seq = seq;
        t->received = 1;
        t->expected = 1;
        return;
    }
    t->received++;

    if (seq == t->last_seq) {
        t->duplicates++;
        return;
    }
    if (seq < t->last_seq) {
        /* Out of order rather than missing. Counted separately because the two
         * have different causes: a reorder is the transport shuffling, a gap
         * is the transport losing. */
        t->reorders++;
        return;
    }
    uint32_t step = seq - t->last_seq;
    if (step > 1)
        t->gaps += step - 1;
    t->last_seq = seq;
    t->expected = t->last_seq - t->first_seq + 1u;
}

bool p04_transport_whole(const p04_transport *t)
{
    if (t == NULL || !t->started)
        return false;
    return t->gaps == 0 && t->duplicates == 0 && t->reorders == 0;
}
