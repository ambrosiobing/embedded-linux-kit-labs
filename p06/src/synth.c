/* Frames with known contents, and lines that are wrong in known ways.
 *
 * The point of the generator is that the answer is known before the program
 * runs: a hand covering six zones must read as six occupied zones, and a
 * short line must read as one drop and no frame. Both are assertions the
 * suite can make without a sensor, a Nucleo or a wall.
 *
 * Everything is a pure function of the frame index, the zone index and the
 * seed, so the same request twice gives the same bytes, and a fault injected
 * into frame twenty changes frame twenty and nothing else.
 */

#include "p06.h"

#include <string.h>

void p06_synth_init(p06_synth *s)
{
    if (s == NULL)
        return;
    memset(s, 0, sizeof *s);
    s->frames = 20;
    s->scene = P06_SCENE_EMPTY;
    s->wall_mm = 1800;
    s->hand_mm = 300;
    s->hand_zones = 6;
    s->noise_mm = 5;
    s->seed = 1;
}

p06_status p06_synth_add_fault(p06_synth *s, p06_fault_kind kind, uint32_t at)
{
    if (s == NULL)
        return P06_E_ARG;
    if (s->fault_count >= P06_FAULTS_MAX)
        return P06_E_RANGE;
    s->faults[s->fault_count].kind = kind;
    s->faults[s->fault_count].at = at;
    s->fault_count++;
    return P06_OK;
}

static bool fault_at(const p06_synth *s, uint32_t frame, p06_fault_kind kind)
{
    for (uint16_t i = 0; i < s->fault_count; i++)
        if (s->faults[i].at == frame && s->faults[i].kind == kind)
            return true;
    return false;
}

static uint32_t mix(uint32_t x)
{
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

/* Plus or minus the noise, evenly, from the frame and the zone alone. */
static int wobble(uint32_t frame, uint16_t zone, uint32_t seed, uint16_t noise)
{
    if (noise == 0)
        return 0;
    uint32_t h = mix(mix(frame * 2654435761u) ^ mix(zone + seed * 40503u));
    uint32_t span = (uint32_t)noise * 2u + 1u;
    return (int)(h % span) - (int)noise;
}

/* Where the hand sits in this frame, as a first zone and a count. The patch
 * is contiguous in zone order, which is a simplification and is why the
 * suite asserts the zone count rather than a shape. */
static uint16_t hand_start(const p06_synth *s, uint32_t frame)
{
    if (s->scene == P06_SCENE_WALK)
        return (uint16_t)((20u + frame) % (uint32_t)(P06_ZONES - s->hand_zones + 1));
    return 27;
}

static void frame_of(const p06_synth *s, uint32_t frame, p06_frame *f)
{
    for (uint16_t z = 0; z < P06_ZONES; z++) {
        int v = (int)s->wall_mm + wobble(frame, z, s->seed, s->noise_mm);
        if (v < 0)
            v = 0;
        f->mm[z] = (uint16_t)v;
    }
    if (s->scene == P06_SCENE_EMPTY || s->hand_zones == 0)
        return;

    uint16_t start = hand_start(s, frame);
    for (uint16_t i = 0; i < s->hand_zones; i++) {
        uint16_t z = (uint16_t)(start + i);
        if (z >= P06_ZONES)
            break;
        int v = (int)s->hand_mm + wobble(frame, (uint16_t)(z + 101u), s->seed, s->noise_mm);
        if (v < 0)
            v = 0;
        f->mm[z] = (uint16_t)v;
    }
}

static int write_line(FILE *fp, const p06_frame *f, uint16_t fields)
{
    int n = fprintf(fp, "Z");
    for (uint16_t z = 0; z < fields && n >= 0; z++)
        n = fprintf(fp, ",%u", (unsigned)f->mm[z]);
    if (n >= 0)
        n = fprintf(fp, "\n");
    return n;
}

p06_status p06_synth_write(const p06_synth *s, const char *path)
{
    if (s == NULL || path == NULL)
        return P06_E_ARG;
    if (s->hand_zones > P06_ZONES)
        return P06_E_RANGE;

    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return P06_E_IO;

    int bad = 0;
    p06_frame f;

    for (uint32_t i = 0; i < s->frames && bad >= 0; i++) {
        frame_of(s, i, &f);

        if (fault_at(s, i, P06_FAULT_NOT_Z)) {
            /* A line from the other firmware, or from the ST-LINK node. */
            bad = fprintf(fp, "INFO,boot complete\n");
            continue;
        }
        if (fault_at(s, i, P06_FAULT_SHORT)) {
            bad = write_line(fp, &f, P06_ZONES - 1);
            continue;
        }
        if (fault_at(s, i, P06_FAULT_RUBBISH)) {
            bad = fprintf(fp, "Z");
            for (uint16_t z = 0; z < P06_ZONES && bad >= 0; z++)
                bad = (z == 10) ? fprintf(fp, ",--")
                                : fprintf(fp, ",%u", (unsigned)f.mm[z]);
            if (bad >= 0)
                bad = fprintf(fp, "\n");
            continue;
        }
        if (fault_at(s, i, P06_FAULT_HUGE)) {
            bad = fprintf(fp, "Z");
            for (uint16_t z = 0; z < P06_ZONES && bad >= 0; z++)
                bad = (z == 10) ? fprintf(fp, ",%u", (unsigned)(P06_RANGE_MAX_MM + 1))
                                : fprintf(fp, ",%u", (unsigned)f.mm[z]);
            if (bad >= 0)
                bad = fprintf(fp, "\n");
            continue;
        }
        if (fault_at(s, i, P06_FAULT_LONG)) {
            char pad[LINEREADER_MAX + 64];
            memset(pad, 'x', sizeof pad - 1);
            pad[sizeof pad - 1] = '\0';
            bad = fprintf(fp, "Z,%s\n", pad);
            continue;
        }

        bad = write_line(fp, &f, P06_ZONES);
    }

    if (fclose(fp) != 0 || bad < 0)
        return P06_E_IO;
    return P06_OK;
}
