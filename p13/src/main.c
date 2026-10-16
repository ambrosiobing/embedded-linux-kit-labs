/* P13 on the command line.
 *
 * Exit status. Zero means the question was answered. Two means the command
 * line was wrong. One means either the question could not be answered, with
 * the reason on standard error, or the acceptance test was applied and
 * failed.
 *
 * supervise reports and exits zero whatever the radios did, because an
 * outage is a result. verify is the acceptance test and fails when the log
 * does not show it, including when the log shows a perfectly healthy run
 * that nobody ever pulled an antenna from.
 */
#define _POSIX_C_SOURCE 200809L

#include "p13.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static void usage(FILE *out)
{
    fprintf(out,
        "usage: p13 <command> [options]\n"
        "\n"
        "  supervise  [--source FILE] [--log FILE] [--window S]\n"
        "  scene      --kind steady|outage|short-gap|no-return|backup-only\n"
        "             --out FILE [--minutes N] [--window S]\n"
        "  replay     --source LOG [--window S]\n"
        "  verify     --source LOG [--window S]\n"
        "\n"
        "supervise reads records on standard input unless --source is given,\n"
        "so the live form is:\n"
        "\n"
        "  mosquitto_sub -h localhost -t lab/p13/health -v | \\\n"
        "      p13 supervise --log bench.p13\n");
}

typedef struct {
    const char *source;
    const char *out;
    const char *log;
    const char *kind;
    double window;
    double minutes;
} opts;

static void opts_init(opts *o)
{
    memset(o, 0, sizeof *o);
    o->window = P13_WINDOW_DEFAULT_S;
    o->minutes = 10.0;
}

#define NEED_VALUE()                                                        \
    do {                                                                    \
        if (i + 1 >= argc) {                                                \
            fprintf(stderr, "p13: %s needs a value\n", a);                  \
            return false;                                                   \
        }                                                                   \
        v = argv[++i];                                                      \
    } while (0)

static bool parse_args(opts *o, int argc, char **argv)
{
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        const char *v = NULL;
        if (strcmp(a, "--source") == 0)       { NEED_VALUE(); o->source = v; }
        else if (strcmp(a, "--out") == 0)     { NEED_VALUE(); o->out = v; }
        else if (strcmp(a, "--log") == 0)     { NEED_VALUE(); o->log = v; }
        else if (strcmp(a, "--kind") == 0)    { NEED_VALUE(); o->kind = v; }
        else if (strcmp(a, "--window") == 0)  { NEED_VALUE(); o->window = atof(v); }
        else if (strcmp(a, "--minutes") == 0) { NEED_VALUE(); o->minutes = atof(v); }
        else {
            fprintf(stderr, "p13: unknown option %s\n", a);
            return false;
        }
    }
    return true;
}

static double now_s(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0.0;
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static int cmd_supervise(const opts *o)
{
    linereader r;
    if (linereader_open(&r, o->source != NULL ? o->source : "-") != LINEREADER_OK) {
        fprintf(stderr, "p13: %s cannot be read\n", o->source);
        return 1;
    }

    p13_log log;
    bool logging = false;
    if (o->log != NULL) {
        if (p13_log_open(&log, o->log) != P13_OK) {
            fprintf(stderr, "p13: %s cannot be written\n", o->log);
            linereader_close(&r);
            return 1;
        }
        logging = true;
    }

    p13_supervisor s;
    p13_init(&s, o->window);
    const double start = now_s();
    printf("state %s, window %.0f s\n", p13_state_name(s.state), s.window_s);

    for (;;) {
        const char *line = NULL;
        size_t len = 0;
        if (linereader_line(&r, &line, &len) != LINEREADER_OK)
            break;

        const double t = now_s() - start;

        /* Every record is an opportunity for the window to have expired,
         * and a stream that goes quiet is exactly when it does. A tick loop
         * belongs beside this on a live bench; here the arrival of the next
         * record is what moves the clock forward. */
        if (p13_on_tick(&s, t) == P13_EVENT_TO_FAILOVER) {
            printf("%8.3f state FAILOVER, the primary is %.1f s late\n",
                   t, t - s.last_primary_at);
            if (logging)
                p13_log_state(&log, t, P13_FAILOVER, t - s.last_primary_at);
        }

        char tag[P13_TAG_MAX];
        if (!p13_tag_of(line, tag, sizeof tag)) {
            s.unreadable++;
            continue;
        }
        if (logging)
            p13_log_record(&log, t, tag);
        if (p13_on_record(&s, t, tag, line) == P13_EVENT_TO_PRIMARY) {
            printf("%8.3f state PRIMARY, on a %s record\n", t, tag);
            if (logging)
                p13_log_state(&log, t, P13_PRIMARY, 0.0);
        }
    }

    linereader_close(&r);
    if (logging)
        p13_log_close(&log);

    printf("\n%lu primary records, %lu backup, %lu unreadable\n",
           (unsigned long)s.primary_records, (unsigned long)s.backup_records,
           (unsigned long)s.unreadable);
    printf("%lu falls to FAILOVER, %lu returns to PRIMARY, ending %s\n",
           (unsigned long)s.to_failover, (unsigned long)s.to_primary,
           p13_state_name(s.state));
    return 0;
}

static int cmd_scene(const opts *o)
{
    if (o->out == NULL || o->kind == NULL) {
        fprintf(stderr, "p13: --kind and --out are both required\n");
        return 2;
    }
    p13_scene scene;
    if (strcmp(o->kind, "steady") == 0)           scene = P13_SCENE_STEADY;
    else if (strcmp(o->kind, "outage") == 0)      scene = P13_SCENE_OUTAGE;
    else if (strcmp(o->kind, "short-gap") == 0)   scene = P13_SCENE_SHORT_GAP;
    else if (strcmp(o->kind, "no-return") == 0)   scene = P13_SCENE_NO_RETURN;
    else if (strcmp(o->kind, "backup-only") == 0) scene = P13_SCENE_BACKUP_ONLY;
    else {
        fprintf(stderr, "p13: --kind must be steady, outage, short-gap, "
                        "no-return or backup-only\n");
        return 2;
    }

    p13_synth cfg;
    p13_synth_init(&cfg);
    cfg.minutes = o->minutes;

    p13_status st = p13_scene_write(scene, &cfg, o->window, o->out);
    if (st != P13_OK) {
        fprintf(stderr, "p13: %s: %s\n", o->out, p13_strerror(st));
        return 1;
    }
    printf("wrote the %s scene, %.0f minutes, to %s\n",
           o->kind, cfg.minutes, o->out);
    return 0;
}

static int cmd_replay(const opts *o)
{
    if (o->source == NULL) {
        fprintf(stderr, "p13: --source is required\n");
        return 2;
    }
    p13_supervisor s;
    p13_status st = p13_replay(o->source, o->window, &s);
    if (st != P13_OK) {
        fprintf(stderr, "p13: %s: %s\n", o->source, p13_strerror(st));
        return 1;
    }
    printf("%lu primary records, %lu backup, %lu unreadable\n",
           (unsigned long)s.primary_records, (unsigned long)s.backup_records,
           (unsigned long)s.unreadable);
    printf("%lu falls to FAILOVER, %lu returns to PRIMARY, ending %s\n",
           (unsigned long)s.to_failover, (unsigned long)s.to_primary,
           p13_state_name(s.state));
    return 0;
}

static int cmd_verify(const opts *o)
{
    if (o->source == NULL) {
        fprintf(stderr, "p13: --source is required\n");
        return 2;
    }
    p13_verdict v;
    p13_status st = p13_verify(o->source, o->window, &v);
    if (st != P13_OK) {
        fprintf(stderr, "p13: %s: %s\n", o->source, p13_strerror(st));
        return 1;
    }
    p13_verdict_print(&v, stdout);
    if (!p13_verdict_passed(&v)) {
        printf("\nThe acceptance test was not met. Each line above is a "
               "separate item of it, and the ones that read FAIL are the "
               "ones this log does not show.\n");
        return 1;
    }
    printf("\nThe acceptance test was met on every count.\n");
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
    if (strcmp(cmd, "supervise") == 0) return cmd_supervise(&o);
    if (strcmp(cmd, "scene") == 0)     return cmd_scene(&o);
    if (strcmp(cmd, "replay") == 0)    return cmd_replay(&o);
    if (strcmp(cmd, "verify") == 0)    return cmd_verify(&o);

    fprintf(stderr, "p13: unknown command %s\n", cmd);
    usage(stderr);
    return 2;
}
