/* P02 on the command line.
 *
 * Exit status. Zero means the question was answered. Two means the command
 * line was wrong. One means either the question could not be answered, with
 * the reason on standard error, or a gate was applied and failed.
 *
 * plan is the gate. It refuses a schedule whose send period sits inside the
 * active window, because such a node never reaches power saving and the lab
 * would then measure the opposite of what it claims. A session reports and
 * exits zero whatever the network did, because an unregistered wake is a
 * result rather than a fault in the program.
 */

#include "p02.h"

#include <stdlib.h>
#include <string.h>

static void usage(FILE *out)
{
    fprintf(out,
        "usage: p02 <command> [options]\n"
        "\n"
        "  timers     [--tau BITS] [--active BITS]\n"
        "  plan       [--tau BITS] [--active BITS] [--period S]\n"
        "  transcript --scene attached|psm-refused|searching|silent --out FILE\n"
        "             [--cycles N]\n"
        "  session    --transcript FILE | --port DEV\n"
        "             [--log FILE] [--cycles N] [--endpoint ADDR] [--port-num N]\n"
        "             [--payload HEX] [--max-payload N]\n"
        "\n"
        "BITS is the eight-character bitmap AT+CPSMS takes. A payload is\n"
        "hexadecimal, so its length on the wire is half the string.\n");
}

typedef struct {
    const char *tau;
    const char *active;
    const char *out;
    const char *scene;
    const char *transcript;
    const char *port;
    const char *log;
    const char *endpoint;
    const char *payload;
    long period;
    long cycles;
    long port_num;
    long max_payload;
} opts;

static void opts_init(opts *o)
{
    memset(o, 0, sizeof *o);
    o->tau = "00100011";
    o->active = "00000101";
    o->period = P02_PERIOD_DEFAULT_S;
    o->cycles = P02_CYCLES_DEFAULT;
    o->port_num = 5683;
    o->max_payload = P02_PAYLOAD_MAX_BYTES;
    o->endpoint = "172.16.0.1";
    o->payload = "50494E47";
}

#define NEED_VALUE()                                                        \
    do {                                                                    \
        if (i + 1 >= argc) {                                                \
            fprintf(stderr, "p02: %s needs a value\n", a);                  \
            return false;                                                   \
        }                                                                   \
        v = argv[++i];                                                      \
    } while (0)

static bool parse_args(opts *o, int argc, char **argv)
{
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        const char *v = NULL;
        if (strcmp(a, "--tau") == 0)             { NEED_VALUE(); o->tau = v; }
        else if (strcmp(a, "--active") == 0)     { NEED_VALUE(); o->active = v; }
        else if (strcmp(a, "--out") == 0)        { NEED_VALUE(); o->out = v; }
        else if (strcmp(a, "--scene") == 0)      { NEED_VALUE(); o->scene = v; }
        else if (strcmp(a, "--transcript") == 0) { NEED_VALUE(); o->transcript = v; }
        else if (strcmp(a, "--port") == 0)       { NEED_VALUE(); o->port = v; }
        else if (strcmp(a, "--log") == 0)        { NEED_VALUE(); o->log = v; }
        else if (strcmp(a, "--endpoint") == 0)   { NEED_VALUE(); o->endpoint = v; }
        else if (strcmp(a, "--payload") == 0)    { NEED_VALUE(); o->payload = v; }
        else if (strcmp(a, "--period") == 0)     { NEED_VALUE(); o->period = atol(v); }
        else if (strcmp(a, "--cycles") == 0)     { NEED_VALUE(); o->cycles = atol(v); }
        else if (strcmp(a, "--port-num") == 0)   { NEED_VALUE(); o->port_num = atol(v); }
        else if (strcmp(a, "--max-payload") == 0){ NEED_VALUE(); o->max_payload = atol(v); }
        else {
            fprintf(stderr, "p02: unknown option %s\n", a);
            return false;
        }
    }
    return true;
}

static void apply(const opts *o, p02_config *c)
{
    p02_config_init(c);
    c->tau_bits = o->tau;
    c->active_bits = o->active;
    c->endpoint = o->endpoint;
    c->payload_hex = o->payload;
    c->port = (uint16_t)o->port_num;
    c->cycles = (uint32_t)(o->cycles > 0 ? o->cycles : 1);
    c->period_s = (uint32_t)(o->period > 0 ? o->period : 1);
    c->max_payload_bytes = (uint32_t)(o->max_payload > 0 ? o->max_payload : 0);
}

static int cmd_timers(const opts *o)
{
    p02_timer tau;
    p02_timer active;
    if (p02_timer_parse(&tau, P02_TIMER_TAU, o->tau) != P02_OK) {
        fprintf(stderr, "p02: %s is not eight characters of 0 and 1\n", o->tau);
        return 2;
    }
    if (p02_timer_parse(&active, P02_TIMER_ACTIVE, o->active) != P02_OK) {
        fprintf(stderr, "p02: %s is not eight characters of 0 and 1\n", o->active);
        return 2;
    }
    p02_timer_print(&tau, P02_TIMER_TAU, stdout);
    p02_timer_print(&active, P02_TIMER_ACTIVE, stdout);
    return 0;
}

static int cmd_plan(const opts *o)
{
    p02_timer tau;
    p02_timer active;
    if (p02_timer_parse(&tau, P02_TIMER_TAU, o->tau) != P02_OK ||
        p02_timer_parse(&active, P02_TIMER_ACTIVE, o->active) != P02_OK) {
        fprintf(stderr, "p02: a timer is not eight characters of 0 and 1\n");
        return 2;
    }
    p02_plan plan;
    if (p02_plan_make(&plan, &tau, &active, (uint32_t)(o->period > 0 ? o->period : 0))
            != P02_OK) {
        fprintf(stderr, "p02: --period must be a positive number of seconds\n");
        return 2;
    }
    p02_plan_print(&plan, stdout);

    if (!plan.radio_ever_sleeps) {
        fprintf(stderr, "p02: this schedule never lets the radio sleep, so the "
                        "lab would measure the opposite of what it claims\n");
        return 1;
    }
    return 0;
}

static int cmd_transcript(const opts *o)
{
    if (o->out == NULL || o->scene == NULL) {
        fprintf(stderr, "p02: --scene and --out are both required\n");
        return 2;
    }
    p02_scene scene;
    if (strcmp(o->scene, "attached") == 0)          scene = P02_SCENE_ATTACHED;
    else if (strcmp(o->scene, "psm-refused") == 0)  scene = P02_SCENE_PSM_REFUSED;
    else if (strcmp(o->scene, "searching") == 0)    scene = P02_SCENE_SEARCHING;
    else if (strcmp(o->scene, "silent") == 0)       scene = P02_SCENE_SILENT;
    else {
        fprintf(stderr, "p02: --scene must be attached, psm-refused, "
                        "searching or silent\n");
        return 2;
    }

    p02_config c;
    apply(o, &c);
    p02_status st = p02_scene_write(scene, &c, o->out);
    if (st != P02_OK) {
        fprintf(stderr, "p02: %s: %s\n", o->out, p02_strerror(st));
        return 1;
    }
    printf("wrote the %s scene, %lu cycles, to %s\n",
           o->scene, (unsigned long)c.cycles, o->out);
    return 0;
}

static int cmd_session(const opts *o)
{
    if ((o->transcript == NULL) == (o->port == NULL)) {
        fprintf(stderr, "p02: give exactly one of --transcript and --port\n");
        return 2;
    }

    p02_config c;
    apply(o, &c);

    uint32_t bytes = 0;
    p02_status st = p02_payload_check(c.payload_hex, c.max_payload_bytes, &bytes);
    if (st == P02_E_FORMAT) {
        fprintf(stderr, "p02: the payload must be an even number of "
                        "hexadecimal digits\n");
        return 2;
    }
    if (st == P02_E_RANGE) {
        fprintf(stderr, "p02: the payload is %lu bytes and the budget is %lu. "
                        "A field node whose record outgrew its plan is the "
                        "failure this lab is written against\n",
                (unsigned long)bytes, (unsigned long)c.max_payload_bytes);
        return 1;
    }

    atchat at;
    atchat_init(&at);
    at_status r = (o->transcript != NULL)
                ? atchat_open_transcript(&at, o->transcript)
                : atchat_open_serial(&at, o->port, 115200);
    if (r != AT_OK) {
        fprintf(stderr, "p02: %s: %s\n",
                o->transcript ? o->transcript : o->port, at_strerror(r));
        return 1;
    }
    if (o->log != NULL && atchat_log_to(&at, o->log) != AT_OK) {
        fprintf(stderr, "p02: %s could not be opened for the log\n", o->log);
        atchat_close(&at);
        return 1;
    }

    p02_session s;
    p02_session_init(&s);

    st = p02_bringup(&at, &c, &s);
    if (st != P02_OK) {
        fprintf(stderr, "p02: bring-up stopped: %s\n", p02_strerror(st));
        p02_session_print(&s, stdout);
        atchat_close(&at);
        return 1;
    }

    for (uint32_t i = 0; i < c.cycles; i++) {
        st = p02_cycle(&at, &c, &s);
        if (st != P02_OK) {
            fprintf(stderr, "p02: cycle %lu stopped: %s\n",
                    (unsigned long)(i + 1), p02_strerror(st));
            break;
        }
    }

    p02_session_print(&s, stdout);

    /* The arithmetic of the schedule, printed beside what the session did,
     * because the acceptance test counts datagrams at the far end and this
     * is the same count worked out in advance. */
    p02_plan plan;
    if (p02_plan_make(&plan, &s.tau_requested, &s.active_requested, c.period_s)
            == P02_OK) {
        printf("\n");
        p02_plan_print(&plan, stdout);
    }

    atchat_close(&at);
    return (st == P02_OK) ? 0 : 1;
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
    if (strcmp(cmd, "timers") == 0)     return cmd_timers(&o);
    if (strcmp(cmd, "plan") == 0)       return cmd_plan(&o);
    if (strcmp(cmd, "transcript") == 0) return cmd_transcript(&o);
    if (strcmp(cmd, "session") == 0)    return cmd_session(&o);

    fprintf(stderr, "p02: unknown command %s\n", cmd);
    usage(stderr);
    return 2;
}
