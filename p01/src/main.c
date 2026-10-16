/* P01 on the command line, and P09 with one argument changed.
 *
 * Exit status. Zero means the question was answered. Two means the command
 * line was wrong. One means the question could not be answered, with the
 * reason on standard error.
 *
 * run is the only subcommand that needs hardware, and on a machine with no
 * bus it says so rather than reporting zero events. The others are the lab
 * with the sensor still in its bag.
 */
#define _POSIX_C_SOURCE 200809L

#include "p01.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static void usage(FILE *out)
{
    fprintf(out,
        "usage: p01 <command> [options]\n"
        "\n"
        "  decode  --bytes \"00 00 00 00 00 01\"\n"
        "  scene   --kind rest|knock|ringing|on-its-side|free-fall\n"
        "          --out FILE [--seconds S] [--rate HZ]\n"
        "  replay  --source FILE [--deadband G] [--cooloff S]\n"
        "  budget  [--rate HZ] [--cooloff S] [--fsync S] [--record BYTES]\n"
        "          [--hours N]\n"
        "  run     --bus N [--address 0xNN] [--seconds S] [--deadband G]\n"
        "          [--cooloff S]\n"
        "\n"
        "P01 runs this on bus 1 and P09 on bus 0. That is the whole of the\n"
        "difference between the two labs' publishers, so it is an argument\n"
        "rather than a second program.\n");
}

typedef struct {
    const char *bytes;
    const char *out;
    const char *kind;
    const char *source;
    long bus;
    long address;
    double seconds;
    double rate;
    double deadband;
    double cooloff;
    double fsync_s;
    long record;
    double hours;
    bool have_bus;
} opts;

static void opts_init(opts *o)
{
    memset(o, 0, sizeof *o);
    o->address = P01_ADDRESS;
    o->seconds = 10.0;
    o->rate = P01_SAMPLE_HZ;
    o->deadband = P01_DEADBAND_G;
    o->cooloff = P01_COOLOFF_S;
    o->fsync_s = P01_FSYNC_S;
    o->record = P01_RECORD_BYTES;
    o->hours = 1.0;
}

#define NEED_VALUE()                                                        \
    do {                                                                    \
        if (i + 1 >= argc) {                                                \
            fprintf(stderr, "p01: %s needs a value\n", a);                  \
            return false;                                                   \
        }                                                                   \
        v = argv[++i];                                                      \
    } while (0)

static bool parse_args(opts *o, int argc, char **argv)
{
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        const char *v = NULL;
        if (strcmp(a, "--bytes") == 0)         { NEED_VALUE(); o->bytes = v; }
        else if (strcmp(a, "--out") == 0)      { NEED_VALUE(); o->out = v; }
        else if (strcmp(a, "--kind") == 0)     { NEED_VALUE(); o->kind = v; }
        else if (strcmp(a, "--source") == 0)   { NEED_VALUE(); o->source = v; }
        else if (strcmp(a, "--bus") == 0)      { NEED_VALUE(); o->bus = atol(v);
                                                 o->have_bus = true; }
        else if (strcmp(a, "--address") == 0)  { NEED_VALUE();
                                                 o->address = strtol(v, NULL, 0); }
        else if (strcmp(a, "--seconds") == 0)  { NEED_VALUE(); o->seconds = atof(v); }
        else if (strcmp(a, "--rate") == 0)     { NEED_VALUE(); o->rate = atof(v); }
        else if (strcmp(a, "--deadband") == 0) { NEED_VALUE(); o->deadband = atof(v); }
        else if (strcmp(a, "--cooloff") == 0)  { NEED_VALUE(); o->cooloff = atof(v); }
        else if (strcmp(a, "--fsync") == 0)    { NEED_VALUE(); o->fsync_s = atof(v); }
        else if (strcmp(a, "--record") == 0)   { NEED_VALUE(); o->record = atol(v); }
        else if (strcmp(a, "--hours") == 0)    { NEED_VALUE(); o->hours = atof(v); }
        else {
            fprintf(stderr, "p01: unknown option %s\n", a);
            return false;
        }
    }
    return true;
}

static int cmd_decode(const opts *o)
{
    if (o->bytes == NULL) {
        fprintf(stderr, "p01: --bytes is required\n");
        return 2;
    }
    uint8_t raw[6];
    if (!p01_parse_bytes(o->bytes, raw)) {
        fprintf(stderr, "p01: that is not six byte values\n");
        return 2;
    }
    p01_sample s;
    p01_decode(raw, &s);
    printf("ax %+.4f g, ay %+.4f g, az %+.4f g\n", s.ax, s.ay, s.az);
    printf("magnitude %.4f g, %.4f from rest, dead band %.2f\n",
           s.magnitude, s.magnitude - 1.0, o->deadband);
    return 0;
}

static int cmd_scene(const opts *o)
{
    if (o->out == NULL || o->kind == NULL) {
        fprintf(stderr, "p01: --kind and --out are both required\n");
        return 2;
    }
    p01_scene scene;
    if (strcmp(o->kind, "rest") == 0)              scene = P01_SCENE_AT_REST;
    else if (strcmp(o->kind, "knock") == 0)        scene = P01_SCENE_ONE_KNOCK;
    else if (strcmp(o->kind, "ringing") == 0)      scene = P01_SCENE_RINGING;
    else if (strcmp(o->kind, "on-its-side") == 0)  scene = P01_SCENE_ON_ITS_SIDE;
    else if (strcmp(o->kind, "free-fall") == 0)    scene = P01_SCENE_FREE_FALL;
    else {
        fprintf(stderr, "p01: --kind must be rest, knock, ringing, "
                        "on-its-side or free-fall\n");
        return 2;
    }
    p01_status st = p01_scene_write(scene, o->seconds, o->rate, o->out);
    if (st != P01_OK) {
        fprintf(stderr, "p01: %s: %s\n", o->out, p01_strerror(st));
        return 1;
    }
    printf("wrote %.0f s of the %s scene at %.0f Hz to %s\n",
           o->seconds, o->kind, o->rate, o->out);
    return 0;
}

static int cmd_replay(const opts *o)
{
    if (o->source == NULL) {
        fprintf(stderr, "p01: --source is required\n");
        return 2;
    }
    p01_run r;
    p01_status st = p01_replay(o->source, o->deadband, o->cooloff, &r);
    if (st != P01_OK) {
        fprintf(stderr, "p01: %s: %s\n", o->source, p01_strerror(st));
        return 1;
    }
    p01_run_print(&r, stdout);
    return 0;
}

static int cmd_budget(const opts *o)
{
    p01_budget b;
    if (p01_budget_make(&b, o->rate, o->cooloff, o->fsync_s,
                        (uint32_t)(o->record > 0 ? o->record : 0),
                        o->hours) != P01_OK) {
        fprintf(stderr, "p01: the rate, the cool-off, the fsync interval and "
                        "the hours must all be positive\n");
        return 2;
    }
    p01_budget_print(&b, stdout);
    return 0;
}

static double now_s(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0.0;
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static int cmd_run(const opts *o)
{
    if (!o->have_bus) {
        fprintf(stderr, "p01: --bus is required. It is 1 on a Raspberry Pi "
                        "and 0 on the NanoPi NEO Air, and a command copied "
                        "from the other lab reports an empty bus\n");
        return 2;
    }

    int fd = -1;
    p01_status st = p01_device_open(&fd, (int)o->bus, (uint8_t)o->address);
    if (st != P01_OK) {
        fprintf(stderr, "p01: /dev/i2c-%ld did not open at address 0x%02lx. "
                        "Run i2cdetect on that bus first: an absent device "
                        "and an absent bus look the same from here\n",
                o->bus, (unsigned long)o->address);
        return 1;
    }
    if (p01_device_configure(fd) != P01_OK) {
        fprintf(stderr, "p01: the device did not accept its configuration\n");
        p01_device_close(fd);
        return 1;
    }

    p01_policy p;
    p01_policy_init(&p, o->deadband, o->cooloff);
    const double start = now_s();
    const double period = (o->rate > 0.0) ? 1.0 / o->rate : 0.05;
    struct timespec gap;
    gap.tv_sec = (time_t)period;
    gap.tv_nsec = (long)((period - (double)gap.tv_sec) * 1000000000.0);

    uint32_t read_errors = 0;
    for (;;) {
        const double t = now_s() - start;
        if (o->seconds > 0.0 && t >= o->seconds)
            break;

        uint8_t raw[6];
        if (p01_device_read(fd, raw) != P01_OK) {
            /* Counted, not interpolated. A bus that drops a read is a fact
             * about the bus, and inventing a sample would hide it. */
            read_errors++;
            nanosleep(&gap, NULL);
            continue;
        }
        p01_sample s;
        p01_decode(raw, &s);
        if (p01_policy_offer(&p, t, &s))
            printf("%8.3f event  magnitude %.3f g  ax %+.3f ay %+.3f az %+.3f\n",
                   t, s.magnitude, s.ax, s.ay, s.az);
        nanosleep(&gap, NULL);
    }

    p01_device_close(fd);
    printf("\n%lu samples, %lu past the dead band, %lu published, "
           "%lu suppressed\n",
           (unsigned long)p.samples, (unsigned long)p.over_threshold,
           (unsigned long)p.events, (unsigned long)p.suppressed);
    if (read_errors > 0)
        printf("%lu reads failed on the bus and were counted rather than "
               "filled in\n", (unsigned long)read_errors);
    return 0;
}

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
    if (strcmp(cmd, "decode") == 0) return cmd_decode(&o);
    if (strcmp(cmd, "scene") == 0)  return cmd_scene(&o);
    if (strcmp(cmd, "replay") == 0) return cmd_replay(&o);
    if (strcmp(cmd, "budget") == 0) return cmd_budget(&o);
    if (strcmp(cmd, "run") == 0)    return cmd_run(&o);

    fprintf(stderr, "p01: unknown command %s\n", cmd);
    usage(stderr);
    return 2;
}
