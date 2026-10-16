/* P06 on the command line.
 *
 * Exit status. Zero means the question was answered. Two means the command
 * line was wrong. One means either the question could not be answered, with
 * the reason on standard error, or a gate was applied and failed.
 *
 * Two gates exist and both are the chapter's acceptance test rather than an
 * invention: baseline refuses an unstable wall, and run refuses to report
 * success when --expect-zones is given and the occupied count falls outside
 * it. Without that option run reports and exits zero, because counting is
 * not judging.
 */

#include "p06.h"

#include <stdlib.h>
#include <string.h>

static void usage(FILE *out)
{
    fprintf(out,
        "usage: p06 <command> [options]\n"
        "\n"
        "  synth    --out FILE [--frames N] [--scene empty|hand|walk]\n"
        "           [--wall MM] [--hand MM] [--hand-zones N] [--noise MM]\n"
        "           [--seed N] [--fault KIND:FRAME ...]\n"
        "  baseline --source FILE --out FILE [--placement TEXT]\n"
        "           [--margin MM] [--stability MM]\n"
        "  show     --source FILE [--book FILE] [--frames N]\n"
        "  run      --source FILE [--book FILE] [--frames N]\n"
        "           [--fb PATH] [--fb-sysfs DIR | --geometry WxHxBPP]\n"
        "           [--expect-zones MIN:MAX]\n"
        "\n"
        "A fault KIND is short, notz, rubbish, huge or long. A source of -\n"
        "is standard input, so the recorder reads a character device, a file\n"
        "or a pipe without knowing the difference.\n");
}

typedef struct {
    const char *source;
    const char *out;
    const char *book;
    const char *placement;
    const char *fb;
    const char *fb_sysfs;
    const char *geometry;
    const char *scene;
    const char *expect;
    long frames;
    long wall;
    long hand;
    long hand_zones;
    long noise;
    long seed;
    long margin;
    long stability;
    const char *faults[P06_FAULTS_MAX];
    int fault_count;
} opts;

static void opts_init(opts *o)
{
    memset(o, 0, sizeof *o);
    o->frames = 20;
    o->wall = 1800;
    o->hand = 300;
    o->hand_zones = 6;
    o->noise = 5;
    o->seed = 1;
    o->margin = P06_MARGIN_MM;
    o->stability = P06_STABILITY_LIMIT_MM;
    o->placement = "not stated";
}

#define NEED_VALUE()                                                        \
    do {                                                                    \
        if (i + 1 >= argc) {                                                \
            fprintf(stderr, "p06: %s needs a value\n", a);                  \
            return false;                                                   \
        }                                                                   \
        v = argv[++i];                                                      \
    } while (0)

static bool parse_args(opts *o, int argc, char **argv)
{
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        const char *v = NULL;
        if (strcmp(a, "--source") == 0)            { NEED_VALUE(); o->source = v; }
        else if (strcmp(a, "--out") == 0)          { NEED_VALUE(); o->out = v; }
        else if (strcmp(a, "--book") == 0)         { NEED_VALUE(); o->book = v; }
        else if (strcmp(a, "--placement") == 0)    { NEED_VALUE(); o->placement = v; }
        else if (strcmp(a, "--fb") == 0)           { NEED_VALUE(); o->fb = v; }
        else if (strcmp(a, "--fb-sysfs") == 0)     { NEED_VALUE(); o->fb_sysfs = v; }
        else if (strcmp(a, "--geometry") == 0)     { NEED_VALUE(); o->geometry = v; }
        else if (strcmp(a, "--scene") == 0)        { NEED_VALUE(); o->scene = v; }
        else if (strcmp(a, "--expect-zones") == 0) { NEED_VALUE(); o->expect = v; }
        else if (strcmp(a, "--frames") == 0)       { NEED_VALUE(); o->frames = atol(v); }
        else if (strcmp(a, "--wall") == 0)         { NEED_VALUE(); o->wall = atol(v); }
        else if (strcmp(a, "--hand") == 0)         { NEED_VALUE(); o->hand = atol(v); }
        else if (strcmp(a, "--hand-zones") == 0)   { NEED_VALUE(); o->hand_zones = atol(v); }
        else if (strcmp(a, "--noise") == 0)        { NEED_VALUE(); o->noise = atol(v); }
        else if (strcmp(a, "--seed") == 0)         { NEED_VALUE(); o->seed = atol(v); }
        else if (strcmp(a, "--margin") == 0)       { NEED_VALUE(); o->margin = atol(v); }
        else if (strcmp(a, "--stability") == 0)    { NEED_VALUE(); o->stability = atol(v); }
        else if (strcmp(a, "--fault") == 0) {
            NEED_VALUE();
            if (o->fault_count >= P06_FAULTS_MAX) {
                fprintf(stderr, "p06: more than %d faults asked for\n", P06_FAULTS_MAX);
                return false;
            }
            o->faults[o->fault_count++] = v;
        }
        else {
            fprintf(stderr, "p06: unknown option %s\n", a);
            return false;
        }
    }
    return true;
}

static bool require(const char *name, const char *value)
{
    if (value != NULL)
        return true;
    fprintf(stderr, "p06: %s is required\n", name);
    return false;
}

/* ---------------------------------------------------------------- synth */

static bool kind_of(const char *name, p06_fault_kind *kind)
{
    if (strcmp(name, "short") == 0)   { *kind = P06_FAULT_SHORT;   return true; }
    if (strcmp(name, "notz") == 0)    { *kind = P06_FAULT_NOT_Z;   return true; }
    if (strcmp(name, "rubbish") == 0) { *kind = P06_FAULT_RUBBISH; return true; }
    if (strcmp(name, "huge") == 0)    { *kind = P06_FAULT_HUGE;    return true; }
    if (strcmp(name, "long") == 0)    { *kind = P06_FAULT_LONG;    return true; }
    return false;
}

static int cmd_synth(const opts *o)
{
    if (!require("--out", o->out))
        return 2;

    p06_synth s;
    p06_synth_init(&s);
    s.frames = (uint32_t)(o->frames > 0 ? o->frames : 0);
    s.wall_mm = (uint16_t)o->wall;
    s.hand_mm = (uint16_t)o->hand;
    s.hand_zones = (uint16_t)o->hand_zones;
    s.noise_mm = (uint16_t)o->noise;
    s.seed = (uint32_t)o->seed;
    s.scene = P06_SCENE_EMPTY;

    if (o->scene != NULL) {
        if (strcmp(o->scene, "empty") == 0)      s.scene = P06_SCENE_EMPTY;
        else if (strcmp(o->scene, "hand") == 0)  s.scene = P06_SCENE_HAND;
        else if (strcmp(o->scene, "walk") == 0)  s.scene = P06_SCENE_WALK;
        else {
            fprintf(stderr, "p06: --scene must be empty, hand or walk\n");
            return 2;
        }
    }

    for (int i = 0; i < o->fault_count; i++) {
        char name[32];
        unsigned at = 0;
        const char *colon = strchr(o->faults[i], ':');
        if (colon == NULL || (size_t)(colon - o->faults[i]) >= sizeof name) {
            fprintf(stderr, "p06: --fault takes KIND:FRAME, not %s\n", o->faults[i]);
            return 2;
        }
        size_t n = (size_t)(colon - o->faults[i]);
        memcpy(name, o->faults[i], n);
        name[n] = '\0';
        at = (unsigned)atoi(colon + 1);

        p06_fault_kind kind;
        if (!kind_of(name, &kind)) {
            fprintf(stderr, "p06: %s is not a fault kind\n", name);
            return 2;
        }
        if (p06_synth_add_fault(&s, kind, at) != P06_OK) {
            fprintf(stderr, "p06: too many faults\n");
            return 2;
        }
    }

    p06_status st = p06_synth_write(&s, o->out);
    if (st != P06_OK) {
        fprintf(stderr, "p06: %s: %s\n", o->out, p06_strerror(st));
        return 1;
    }
    printf("wrote %lu frames, %u faults, to %s\n",
           (unsigned long)s.frames, (unsigned)s.fault_count, o->out);
    return 0;
}

/* ---------------------------------------------------------------- baseline */

static int cmd_baseline(const opts *o)
{
    if (!require("--source", o->source) || !require("--out", o->out))
        return 2;

    linereader r;
    if (linereader_open(&r, o->source) != LINEREADER_OK) {
        fprintf(stderr, "p06: %s: %s\n", o->source, p06_strerror(P06_E_IO));
        return 1;
    }

    p06_book b;
    p06_book_init(&b);
    snprintf(b.placement, sizeof b.placement, "%s", o->placement);

    uint32_t refused = 0;
    uint32_t dropped = 0;
    for (;;) {
        const char *line = NULL;
        size_t len = 0;
        if (linereader_line(&r, &line, &len) != LINEREADER_OK)
            break;
        p06_frame f;
        if (p06_parse_frame(line, &f, NULL) != P06_OK) {
            dropped++;
            continue;
        }
        if (p06_book_add(&b, &f) != P06_OK)
            refused++;
    }
    linereader_close(&r);

    printf("%lu frames measured, %lu refused for a zone with no target, "
           "%lu lines dropped\n",
           (unsigned long)b.frames, (unsigned long)refused,
           (unsigned long)dropped);

    p06_status st = p06_book_finish(&b, (uint16_t)o->margin, (uint16_t)o->stability);
    if (st != P06_OK) {
        if (b.frames < P06_BASELINE_MIN_FRAMES)
            fprintf(stderr, "p06: %lu frames is not a baseline. This needs at "
                            "least %d, with the field of view empty\n",
                    (unsigned long)b.frames, P06_BASELINE_MIN_FRAMES);
        else if (b.worst_spread_mm > (uint16_t)o->stability)
            fprintf(stderr, "p06: the wall moved by %u mm during the "
                            "measurement, and the limit is %ld. Something was "
                            "in the field of view, or the sensor was not "
                            "still. This is not a threshold yet\n",
                    (unsigned)b.worst_spread_mm, o->stability);
        else
            fprintf(stderr, "p06: the nearest wall at %u mm is not further "
                            "than the margin of %ld mm, so no threshold can "
                            "sit between them\n",
                    (unsigned)b.nearest_wall_mm, o->margin);
        return 1;
    }

    st = p06_book_write(&b, o->out);
    if (st != P06_OK) {
        fprintf(stderr, "p06: %s: %s\n", o->out, p06_strerror(st));
        return 1;
    }
    printf("wall %u mm at the nearest zone, worst spread %u mm, "
           "threshold %u mm, written to %s\n",
           (unsigned)b.nearest_wall_mm, (unsigned)b.worst_spread_mm,
           (unsigned)b.threshold_mm, o->out);
    return 0;
}

/* ---------------------------------------------------------------- show */

static int cmd_show(const opts *o)
{
    if (!require("--source", o->source))
        return 2;

    p06_book b;
    p06_book_init(&b);
    bool have_book = false;
    if (o->book != NULL) {
        if (p06_book_read(&b, o->book) != P06_OK) {
            fprintf(stderr, "p06: %s cannot be read as a lab book\n", o->book);
            return 1;
        }
        have_book = true;
    }

    linereader r;
    if (linereader_open(&r, o->source) != LINEREADER_OK) {
        fprintf(stderr, "p06: %s: %s\n", o->source, p06_strerror(P06_E_IO));
        return 1;
    }

    uint32_t shown = 0;
    const uint32_t limit = (uint32_t)(o->frames > 0 ? o->frames : 1);
    for (;;) {
        const char *line = NULL;
        size_t len = 0;
        if (linereader_line(&r, &line, &len) != LINEREADER_OK)
            break;
        p06_frame f;
        p06_drop why = P06_DROP_NONE;
        if (p06_parse_frame(line, &f, &why) != P06_OK)
            continue;

        uint16_t with_target = 0;
        uint16_t near = p06_frame_near_mm(&f, &with_target);
        printf("frame %lu: near %u mm, far %u mm, %u zones with a target",
               (unsigned long)shown, (unsigned)near,
               (unsigned)p06_frame_far_mm(&f), (unsigned)with_target);
        if (have_book) {
            p06_occupancy occ;
            if (p06_classify(&f, &b, &occ) == P06_OK)
                printf(", %u occupied", (unsigned)occ.count);
        }
        printf("\n");
        p06_frame_print(&f, have_book ? &b : NULL, stdout);
        printf("\n");

        if (++shown >= limit)
            break;
    }
    linereader_close(&r);

    if (shown == 0) {
        fprintf(stderr, "p06: no line in %s was a frame\n", o->source);
        return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------- run */

static int open_glass(const opts *o, p06_fb *fb, bool *opened)
{
    *opened = false;
    if (o->fb == NULL)
        return 0;

    p06_geometry g;
    p06_status st;
    if (o->geometry != NULL)
        st = p06_geometry_parse(&g, o->geometry);
    else if (o->fb_sysfs != NULL)
        st = p06_geometry_from_sysfs(&g, o->fb_sysfs);
    else {
        fprintf(stderr, "p06: --fb needs --fb-sysfs or --geometry. The size "
                        "of a display is not something to assume\n");
        return 2;
    }
    if (st != P06_OK) {
        fprintf(stderr, "p06: the geometry could not be read: %s\n",
                p06_strerror(st));
        return 1;
    }

    st = p06_fb_open(fb, o->fb, &g);
    if (st != P06_OK) {
        fprintf(stderr, "p06: %s: %s\n", o->fb, p06_strerror(st));
        return 1;
    }
    printf("painting %ux%u at %u bits per pixel, %lu bytes\n",
           (unsigned)g.width, (unsigned)g.height, (unsigned)g.bpp,
           (unsigned long)fb->bytes);
    *opened = true;
    return 0;
}

static int cmd_run(const opts *o)
{
    if (!require("--source", o->source))
        return 2;

    p06_book b;
    p06_book_init(&b);
    bool have_book = false;
    if (o->book != NULL) {
        if (p06_book_read(&b, o->book) != P06_OK) {
            fprintf(stderr, "p06: %s cannot be read as a lab book\n", o->book);
            return 1;
        }
        have_book = true;
    }

    p06_fb fb;
    p06_fb_init(&fb);
    bool painting = false;
    int rc = open_glass(o, &fb, &painting);
    if (rc != 0)
        return rc;

    p06_run run;
    p06_run_init(&run);
    p06_status st = p06_run_stream(o->source, have_book ? &b : NULL,
                                   painting ? &fb : NULL,
                                   (uint32_t)(o->frames > 0 ? o->frames : 0),
                                   &run);
    if (painting)
        p06_fb_close(&fb);

    if (st != P06_OK) {
        fprintf(stderr, "p06: %s: %s\n", o->source, p06_strerror(st));
        return 1;
    }

    printf("frames %lu accepted, %lu painted\n",
           (unsigned long)run.accepted, (unsigned long)run.painted);
    printf("dropped %lu: %lu not a Z line, %lu wrong field count, "
           "%lu not an integer, %lu out of range\n",
           (unsigned long)p06_run_dropped_total(&run),
           (unsigned long)run.dropped[P06_DROP_NOT_Z],
           (unsigned long)run.dropped[P06_DROP_FIELD_COUNT],
           (unsigned long)run.dropped[P06_DROP_NOT_INTEGER],
           (unsigned long)run.dropped[P06_DROP_OUT_OF_RANGE]);
    printf("lines refused by the reader: %lu over-long, %lu blank\n",
           (unsigned long)run.long_lines, (unsigned long)run.blank_lines);
    printf("nearest target %u mm to %u mm across the run\n",
           (unsigned)run.near_min_mm, (unsigned)run.near_max_mm);

    if (!have_book) {
        printf("no lab book given, so nothing was classified. Measure the "
               "wall first: p06 baseline\n");
        return 0;
    }
    printf("occupied zones %u to %u, last %u, against a threshold of %u mm\n",
           (unsigned)run.min_occupied, (unsigned)run.max_occupied,
           (unsigned)run.last_occupied, (unsigned)b.threshold_mm);

    if (o->expect == NULL)
        return 0;

    unsigned lo = 0, hi = 0;
    if (sscanf(o->expect, "%u:%u", &lo, &hi) != 2) {
        fprintf(stderr, "p06: --expect-zones takes MIN:MAX\n");
        return 2;
    }
    if (run.accepted == 0) {
        fprintf(stderr, "p06: no frame arrived, so the expectation could not "
                        "be tested. That is not a pass\n");
        return 1;
    }
    if (run.max_occupied < lo || run.max_occupied > hi) {
        printf("FAIL the busiest frame occupied %u zones, and %u to %u was "
               "expected\n", (unsigned)run.max_occupied, lo, hi);
        return 1;
    }
    printf("pass the busiest frame occupied %u zones, within the %u to %u "
           "expected\n", (unsigned)run.max_occupied, lo, hi);
    return 0;
}

/* ---------------------------------------------------------------- main */

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage(stderr);
        return 2;
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage(stdout);
        return 0;
    }

    opts o;
    opts_init(&o);
    if (!parse_args(&o, argc, argv))
        return 2;

    const char *cmd = argv[1];
    if (strcmp(cmd, "synth") == 0)    return cmd_synth(&o);
    if (strcmp(cmd, "baseline") == 0) return cmd_baseline(&o);
    if (strcmp(cmd, "show") == 0)     return cmd_show(&o);
    if (strcmp(cmd, "run") == 0)      return cmd_run(&o);

    fprintf(stderr, "p06: unknown command %s\n", cmd);
    usage(stderr);
    return 2;
}
