/* P03 on the command line.
 *
 * Exit status. Zero means the question was answered. Two means the command
 * line was wrong. One means either the question could not be answered, with
 * the reason on standard error, or a gate was applied and failed.
 *
 * audit is the gate, and session is one as well in one respect: a bearer
 * that is not Cat-M stops it, because every other lab in this volume would
 * publish happily over NB-IoT and this is the one whose whole claim is which
 * bearer carried the payload.
 */

#include "p03.h"

#include <stdlib.h>
#include <string.h>

static void usage(FILE *out)
{
    fprintf(out,
        "usage: p03 <command> [options]\n"
        "\n"
        "  fix        --line \"+CGNSINF: ...\"\n"
        "  transcript --scene fix|cached|no-fix|wrong-bearer|gnss-left-on\n"
        "             --out FILE [--cycles N]\n"
        "  session    --transcript FILE | --port DEV\n"
        "             [--log FILE] [--cycles N] [--broker HOST] [--topic T]\n"
        "  audit      --transcript FILE\n"
        "\n"
        "audit reads any recorded session, including one typed by hand, and\n"
        "answers one question: was the radio asked to transmit while the\n"
        "GNSS receiver was still powered.\n");
}

typedef struct {
    const char *line;
    const char *out;
    const char *scene;
    const char *transcript;
    const char *port;
    const char *log;
    const char *broker;
    const char *topic;
    long cycles;
} opts;

static void opts_init(opts *o)
{
    memset(o, 0, sizeof *o);
    o->cycles = P03_CYCLES_DEFAULT;
}

#define NEED_VALUE()                                                        \
    do {                                                                    \
        if (i + 1 >= argc) {                                                \
            fprintf(stderr, "p03: %s needs a value\n", a);                  \
            return false;                                                   \
        }                                                                   \
        v = argv[++i];                                                      \
    } while (0)

static bool parse_args(opts *o, int argc, char **argv)
{
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        const char *v = NULL;
        if (strcmp(a, "--line") == 0)            { NEED_VALUE(); o->line = v; }
        else if (strcmp(a, "--out") == 0)        { NEED_VALUE(); o->out = v; }
        else if (strcmp(a, "--scene") == 0)      { NEED_VALUE(); o->scene = v; }
        else if (strcmp(a, "--transcript") == 0) { NEED_VALUE(); o->transcript = v; }
        else if (strcmp(a, "--port") == 0)       { NEED_VALUE(); o->port = v; }
        else if (strcmp(a, "--log") == 0)        { NEED_VALUE(); o->log = v; }
        else if (strcmp(a, "--broker") == 0)     { NEED_VALUE(); o->broker = v; }
        else if (strcmp(a, "--topic") == 0)      { NEED_VALUE(); o->topic = v; }
        else if (strcmp(a, "--cycles") == 0)     { NEED_VALUE(); o->cycles = atol(v); }
        else {
            fprintf(stderr, "p03: unknown option %s\n", a);
            return false;
        }
    }
    return true;
}

static void apply(const opts *o, p03_config *c)
{
    p03_config_init(c);
    if (o->broker != NULL)
        c->broker = o->broker;
    if (o->topic != NULL)
        c->topic = o->topic;
    c->cycles = (uint32_t)(o->cycles > 0 ? o->cycles : 1);
}

static int cmd_fix(const opts *o)
{
    if (o->line == NULL) {
        fprintf(stderr, "p03: --line is required\n");
        return 2;
    }
    p03_fix f;
    if (!p03_fix_parse(o->line, &f)) {
        printf("no usable fix in that line, which is the normal answer from a\n"
               "receiver that has been powered for a few seconds\n");
        return 0;
    }
    char payload[128];
    p03_payload(&f, payload, sizeof payload);
    printf("stamp %s, latitude %s, longitude %s\n", f.stamp, f.lat, f.lon);
    printf("payload: %s\n", payload);
    return 0;
}

static int cmd_transcript(const opts *o)
{
    if (o->out == NULL || o->scene == NULL) {
        fprintf(stderr, "p03: --scene and --out are both required\n");
        return 2;
    }
    p03_scene scene;
    if (strcmp(o->scene, "fix") == 0)                scene = P03_SCENE_FIX;
    else if (strcmp(o->scene, "cached") == 0)        scene = P03_SCENE_CACHED;
    else if (strcmp(o->scene, "no-fix") == 0)        scene = P03_SCENE_NO_FIX;
    else if (strcmp(o->scene, "wrong-bearer") == 0)  scene = P03_SCENE_WRONG_BEARER;
    else if (strcmp(o->scene, "gnss-left-on") == 0)  scene = P03_SCENE_GNSS_LEFT_ON;
    else {
        fprintf(stderr, "p03: --scene must be fix, cached, no-fix, "
                        "wrong-bearer or gnss-left-on\n");
        return 2;
    }

    p03_config c;
    apply(o, &c);
    p03_status st = p03_scene_write(scene, &c, o->out);
    if (st != P03_OK) {
        fprintf(stderr, "p03: %s: %s\n", o->out, p03_strerror(st));
        return 1;
    }
    printf("wrote the %s scene to %s\n", o->scene, o->out);
    return 0;
}

static int cmd_session(const opts *o)
{
    if ((o->transcript == NULL) == (o->port == NULL)) {
        fprintf(stderr, "p03: give exactly one of --transcript and --port\n");
        return 2;
    }

    p03_config c;
    apply(o, &c);

    atchat at;
    atchat_init(&at);
    at_status r = (o->transcript != NULL)
                ? atchat_open_transcript(&at, o->transcript)
                : atchat_open_serial(&at, o->port, 115200);
    if (r != AT_OK) {
        fprintf(stderr, "p03: %s: %s\n",
                o->transcript ? o->transcript : o->port, at_strerror(r));
        return 1;
    }
    if (o->log != NULL && atchat_log_to(&at, o->log) != AT_OK) {
        fprintf(stderr, "p03: %s could not be opened for the log\n", o->log);
        atchat_close(&at);
        return 1;
    }

    p03_session s;
    p03_session_init(&s);

    p03_status st = p03_bringup(&at, &c, &s);
    if (st != P03_OK) {
        if (st == P03_E_BEARER)
            fprintf(stderr, "p03: the radio is not on Cat-M. Every command "
                            "after this would have worked, and the payload "
                            "would have gone out over the wrong bearer, which "
                            "is the one thing this lab exists to catch\n");
        else
            fprintf(stderr, "p03: bring-up stopped: %s\n", p03_strerror(st));
        p03_session_print(&s, stdout);
        atchat_close(&at);
        return 1;
    }

    for (uint32_t i = 0; i < c.cycles; i++) {
        st = p03_cycle(&at, &c, &s);
        if (st != P03_OK) {
            fprintf(stderr, "p03: cycle %lu stopped: %s\n",
                    (unsigned long)(i + 1), p03_strerror(st));
            break;
        }
    }

    p03_session_print(&s, stdout);
    atchat_close(&at);
    return (st == P03_OK) ? 0 : 1;
}

static int cmd_audit(const opts *o)
{
    if (o->transcript == NULL) {
        fprintf(stderr, "p03: --transcript is required\n");
        return 2;
    }
    p03_audit a;
    p03_status st = p03_audit_file(o->transcript, &a);
    if (st != P03_OK) {
        fprintf(stderr, "p03: %s: %s\n", o->transcript, p03_strerror(st));
        return 1;
    }

    printf("%lu lines, receiver powered %lu times and down %lu, "
           "%lu transmit commands\n",
           (unsigned long)a.lines, (unsigned long)a.gnss_on,
           (unsigned long)a.gnss_off, (unsigned long)a.transmits);

    if (p03_audit_clean(&a)) {
        printf("pass nothing transmitted while the receiver was powered\n");
        if (a.gnss_on > a.gnss_off)
            printf("note the receiver was left powered at the end of the "
                   "recording, which costs current rather than correctness\n");
        return 0;
    }
    printf("FAIL %lu transmit commands were sent with the receiver still "
           "powered\n", (unsigned long)a.transmits_with_gnss_on);
    printf("     the first was line %lu: %s\n",
           (unsigned long)a.first_bad_line, a.first_bad_cmd);
    return 1;
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
    if (strcmp(cmd, "fix") == 0)        return cmd_fix(&o);
    if (strcmp(cmd, "transcript") == 0) return cmd_transcript(&o);
    if (strcmp(cmd, "session") == 0)    return cmd_session(&o);
    if (strcmp(cmd, "audit") == 0)      return cmd_audit(&o);

    fprintf(stderr, "p03: unknown command %s\n", cmd);
    usage(stderr);
    return 2;
}
