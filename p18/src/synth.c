/* A stream with faults put into it on purpose, so that the recorder can be
 * tested against losses whose size is known in advance.
 *
 * The payload of a sample is a pure function of its sequence number and the
 * seed. That is deliberate and it is what makes a duplicate and a reorder
 * mean what they say: the same sequence number carries the same three values
 * wherever it appears in the file, so a duplicated line is a duplicate of
 * something rather than a second different sample wearing the same number.
 */

#include "p18.h"

#include <string.h>

void p18_synth_init(p18_synth *s)
{
    if (s == NULL)
        return;
    memset(s, 0, sizeof *s);
    s->samples = 100;
    s->start_seq = 0;
    s->seed = 1;
}

p18_status p18_synth_add_fault(p18_synth *s, p18_fault_kind kind, uint32_t at)
{
    if (s == NULL)
        return P18_E_ARG;
    if (s->fault_count >= P18_FAULTS_MAX)
        return P18_E_RANGE;
    s->faults[s->fault_count].kind = kind;
    s->faults[s->fault_count].at = at;
    s->fault_count++;
    return P18_OK;
}

static bool fault_at(const p18_synth *s, uint32_t seq, p18_fault_kind kind)
{
    for (uint16_t i = 0; i < s->fault_count; i++)
        if (s->faults[i].at == seq && s->faults[i].kind == kind)
            return true;
    return false;
}

static uint32_t mix(uint32_t x)
{
    /* xorshift32. Wrapping is the point of it, and unsigned wrapping is
     * defined, which is why every value here is unsigned. */
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

static void sample_of(uint32_t seq, uint32_t seed, int16_t *out3)
{
    uint32_t s = mix(seq ^ (seed * 2654435761u));
    for (int i = 0; i < 3; i++) {
        s = mix(s);
        /* A plausible accelerometer reading in milli-g, from -1000 to 1000. */
        out3[i] = (int16_t)((int32_t)((s >> 8) % 2001u) - 1000);
    }
}

static int write_sample(FILE *fp, uint32_t seq, uint32_t seed)
{
    int16_t a[3];
    sample_of(seq, seed, a);
    return fprintf(fp, "%lu,%d,%d,%d\n", (unsigned long)seq, a[0], a[1], a[2]);
}

static int write_garbled(FILE *fp)
{
    /* No leading digit, so the parser refuses it and counts it. A real stream
     * produces these at the start of a capture, when the reader attaches part
     * way through a line the firmware had already begun. */
    return fprintf(fp, "?? partial line, no sequence number\n");
}

static int write_long(FILE *fp, uint32_t seq)
{
    char pad[P18_LINE_MAX + 64];
    memset(pad, 'x', sizeof pad - 1);
    pad[sizeof pad - 1] = '\0';
    return fprintf(fp, "%lu,%s\n", (unsigned long)seq, pad);
}

p18_status p18_synth_write(const p18_synth *s, const char *path)
{
    if (s == NULL || path == NULL)
        return P18_E_ARG;

    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return P18_E_IO;

    int bad = 0;
    bool skip_next = false;

    for (uint32_t i = 0; i < s->samples && bad >= 0; i++) {
        uint32_t seq = s->start_seq + i;   /* wrapping here is wanted */

        if (skip_next) {
            skip_next = false;
            continue;
        }
        if (fault_at(s, seq, P18_FAULT_DROP))
            continue;
        if (fault_at(s, seq, P18_FAULT_GARBLE)) {
            bad = write_garbled(fp);
            continue;
        }
        if (fault_at(s, seq, P18_FAULT_LONG)) {
            bad = write_long(fp, seq);
            continue;
        }
        if (fault_at(s, seq, P18_FAULT_SWAP) && i + 1 < s->samples) {
            /* The next one first, then this one. The one after is then not
             * written again, which is what makes this a reorder rather than
             * a duplicate followed by a gap. */
            bad = write_sample(fp, seq + 1u, s->seed);
            if (bad >= 0)
                bad = write_sample(fp, seq, s->seed);
            skip_next = true;
            continue;
        }

        bad = write_sample(fp, seq, s->seed);
        if (bad >= 0 && fault_at(s, seq, P18_FAULT_DUP))
            bad = write_sample(fp, seq, s->seed);
    }

    if (fclose(fp) != 0 || bad < 0)
        return P18_E_IO;
    return P18_OK;
}
