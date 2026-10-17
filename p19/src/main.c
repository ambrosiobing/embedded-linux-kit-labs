/* P19 on the command line.
 *
 * Exit status. Zero means the question was answered. Two means the command
 * line was wrong. One means the question could not be answered, with the
 * reason on standard error, or a sweep failed its own acceptance item.
 *
 * There is no subcommand here that opens a socket, and that is the
 * chapter's acceptance item rather than an oversight: no MQTT client runs
 * during this lab, and the simplest way to hold to that is to have nothing
 * that could.
 */
#define _POSIX_C_SOURCE 200809L

#include "p19.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../lib/i2cbus.h"

static void usage(FILE *out)
{
    fprintf(out,
        "usage: p19 <command> [options]\n"
        "\n"
        "  angles  --bytes \"00 00 00 00 00 01\"\n"
        "  sweep   --axis pitch|roll [--step DEG] [--crosstalk DEG]\n"
        "  draw    --fb PATH (--fb-sysfs DIR | --geometry WxHxBPP)\n"
        "          [--pitch DEG] [--roll DEG]\n"
        "  run     --bus N --fb PATH (--fb-sysfs DIR | --geometry WxHxBPP)\n"
        "          [--seconds S]\n"
        "\n"
        "sweep is the acceptance test with no assembly to rotate: it tilts a\n"
        "gravity about one axis and reports how far the other angle moved,\n"
        "which must be nothing.\n");
}

typedef struct {
    const char *bytes;
    const char *axis;
    const char *fb;
    const char *fb_sysfs;
    const char *geometry;
    long bus;
    bool have_bus;
    double pitch;
    double roll;
    double step;
    double crosstalk;
    double seconds;
} opts;

static void opts_init(opts *o)
{
    memset(o, 0, sizeof *o);
    o->step = 1.0;
    o->crosstalk = 0.001;
    o->seconds = 10.0;
}

#define NEED_VALUE()                                                        \
    do {                                                                    \
        if (i + 1 >= argc) {                                                \
            fprintf(stderr, "p19: %s needs a value\n", a);                  \
            return false;                                                   \
        }                                                                   \
        v = argv[++i];                                                      \
    } while (0)

static bool parse_args(opts *o, int argc, char **argv)
{
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        const char *v = NULL;
        if (strcmp(a, "--bytes") == 0)          { NEED_VALUE(); o->bytes = v; }
        else if (strcmp(a, "--axis") == 0)      { NEED_VALUE(); o->axis = v; }
        else if (strcmp(a, "--fb") == 0)        { NEED_VALUE(); o->fb = v; }
        else if (strcmp(a, "--fb-sysfs") == 0)  { NEED_VALUE(); o->fb_sysfs = v; }
        else if (strcmp(a, "--geometry") == 0)  { NEED_VALUE(); o->geometry = v; }
        else if (strcmp(a, "--bus") == 0)       { NEED_VALUE(); o->bus = atol(v);
                                                  o->have_bus = true; }
        else if (strcmp(a, "--pitch") == 0)     { NEED_VALUE(); o->pitch = atof(v); }
        else if (strcmp(a, "--roll") == 0)      { NEED_VALUE(); o->roll = atof(v); }
        else if (strcmp(a, "--step") == 0)      { NEED_VALUE(); o->step = atof(v); }
        else if (strcmp(a, "--crosstalk") == 0) { NEED_VALUE(); o->crosstalk = atof(v); }
        else if (strcmp(a, "--seconds") == 0)   { NEED_VALUE(); o->seconds = atof(v); }
        else {
            fprintf(stderr, "p19: unknown option %s\n", a);
            return false;
        }
    }
    return true;
}

static int cmd_angles(const opts *o)
{
    if (o->bytes == NULL) {
        fprintf(stderr, "p19: --bytes is required\n");
        return 2;
    }
    uint8_t raw[6];
    if (!adxl345_parse_bytes(o->bytes, raw)) {
        fprintf(stderr, "p19: that is not six byte values in hexadecimal\n");
        return 2;
    }
    adxl345_sample s;
    adxl345_decode(raw, &s);
    p19_attitude a;
    p19_angles(&s, &a);

    printf("ax %+.4f g, ay %+.4f g, az %+.4f g, magnitude %.4f\n",
           s.ax, s.ay, s.az, s.magnitude);
    printf("pitch %+.2f degrees, roll %+.2f degrees\n", a.pitch_deg, a.roll_deg);
    if (fabs(s.magnitude - 1.0) > 0.2)
        printf("the magnitude is not one gravity, so the assembly is moving "
               "and these angles describe an acceleration rather than an "
               "attitude\n");
    return 0;
}

static int cmd_sweep(const opts *o)
{
    p19_axis axis;
    if (o->axis == NULL || strcmp(o->axis, "pitch") == 0)
        axis = P19_AXIS_PITCH;
    else if (strcmp(o->axis, "roll") == 0)
        axis = P19_AXIS_ROLL;
    else {
        fprintf(stderr, "p19: --axis must be pitch or roll\n");
        return 2;
    }

    p19_sweep sw;
    if (p19_sweep_axis(axis, -90.0, 90.0, o->step, &sw) != P19_OK) {
        fprintf(stderr, "p19: --step must be positive\n");
        return 2;
    }

    const char *name = (axis == P19_AXIS_PITCH) ? "pitch" : "roll";
    const char *other = (axis == P19_AXIS_PITCH) ? "roll" : "pitch";
    printf("swept %s from -90 to +90 in %.1f degree steps, %lu readings\n",
           name, o->step, (unsigned long)sw.steps);
    printf("%s reached %+.2f degrees, which is full scale\n", name, sw.reached_deg);
    printf("%s moved at most %.6f degrees across the whole sweep\n",
           other, fabs(sw.worst_crosstalk));

    if (fabs(sw.worst_crosstalk) > o->crosstalk) {
        printf("\nFAIL the other bar moved, and the acceptance test asks that "
               "it stay near zero\n");
        return 1;
    }
    printf("\npass the other bar did not move, which is the half of this "
           "test a per-axis instrument fails\n");
    return 0;
}

static int open_glass(const opts *o, fb_target *fb)
{
    if (o->fb == NULL) {
        fprintf(stderr, "p19: --fb is required\n");
        return 2;
    }
    fb_geometry g;
    fb_status st;
    if (o->geometry != NULL)
        st = fb_geometry_parse(&g, o->geometry);
    else if (o->fb_sysfs != NULL)
        st = fb_geometry_from_sysfs(&g, o->fb_sysfs);
    else {
        fprintf(stderr, "p19: --fb needs --fb-sysfs or --geometry. The size "
                        "of a display is not something to assume\n");
        return 2;
    }
    if (st != FB_OK) {
        fprintf(stderr, "p19: the geometry could not be read: %s\n",
                fb_strerror(st));
        return 1;
    }
    fb_init(fb);
    if (fb_open(fb, o->fb, &g) != FB_OK) {
        fprintf(stderr, "p19: %s could not be mapped at %ux%u\n",
                o->fb, (unsigned)g.width, (unsigned)g.height);
        return 1;
    }
    return 0;
}

static int cmd_draw(const opts *o)
{
    fb_target fb;
    int rc = open_glass(o, &fb);
    if (rc != 0)
        return rc;

    p19_attitude a;
    a.pitch_deg = o->pitch;
    a.roll_deg = o->roll;
    p19_status s = p19_paint(&fb, &a);
    fb_close(&fb);

    if (s != P19_OK) {
        fprintf(stderr, "p19: nothing was painted: %s\n", p19_strerror(s));
        return 1;
    }
    printf("painted pitch %+.2f and roll %+.2f on %s\n",
           a.pitch_deg, a.roll_deg, o->fb);
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
        fprintf(stderr, "p19: --bus is required, and it is 1 on the Pi 3 of "
                        "this lab\n");
        return 2;
    }

    fb_target fb;
    int rc = open_glass(o, &fb);
    if (rc != 0)
        return rc;

    int fd = -1;
    if (!i2cbus_open(&fd, (int)o->bus, ADXL345_ADDRESS)) {
        fprintf(stderr, "p19: /dev/i2c-%ld did not open at 0x53. Run "
                        "i2cdetect on that bus first\n", o->bus);
        fb_close(&fb);
        return 1;
    }
    if (!i2cbus_write_reg(fd, ADXL345_REG_POWER_CTL, ADXL345_POWER_MEASURE) ||
        !i2cbus_write_reg(fd, ADXL345_REG_DATA_FORMAT,
                          ADXL345_FORMAT_FULL_RES_16G)) {
        fprintf(stderr, "p19: the device did not accept its configuration\n");
        i2cbus_close(fd);
        fb_close(&fb);
        return 1;
    }

    const double start = now_s();
    const double period = 1.0 / P19_SAMPLE_HZ;
    struct timespec gap;
    gap.tv_sec = (time_t)period;
    gap.tv_nsec = (long)((period - (double)gap.tv_sec) * 1000000000.0);

    uint32_t frames = 0;
    uint32_t read_errors = 0;
    for (;;) {
        if (o->seconds > 0.0 && now_s() - start >= o->seconds)
            break;
        uint8_t raw[6];
        if (!i2cbus_read_reg(fd, ADXL345_REG_DATAX0, raw, 6)) {
            read_errors++;
            nanosleep(&gap, NULL);
            continue;
        }
        adxl345_sample s;
        adxl345_decode(raw, &s);
        p19_attitude a;
        p19_angles(&s, &a);
        if (p19_paint(&fb, &a) == P19_OK)
            frames++;
        nanosleep(&gap, NULL);
    }

    i2cbus_close(fd);
    fb_close(&fb);
    printf("%lu frames painted, %lu reads failed and were counted rather "
           "than filled in\n",
           (unsigned long)frames, (unsigned long)read_errors);
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
    if (strcmp(cmd, "angles") == 0) return cmd_angles(&o);
    if (strcmp(cmd, "sweep") == 0)  return cmd_sweep(&o);
    if (strcmp(cmd, "draw") == 0)   return cmd_draw(&o);
    if (strcmp(cmd, "run") == 0)    return cmd_run(&o);

    fprintf(stderr, "p19: unknown command %s\n", cmd);
    usage(stderr);
    return 2;
}
