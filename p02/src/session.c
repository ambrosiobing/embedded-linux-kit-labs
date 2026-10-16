/* The conversation, as a state machine rather than a sequence of sleeps.
 *
 * Every command waits for its own final line. The indicator is decided from
 * the registration state before the socket is touched and is not revisited,
 * which is the chapter's rule written where it can be checked: a failed send
 * does not darken an indicator that reports whether the network knows this
 * module exists.
 */

#include "p02.h"

#include <stdlib.h>
#include <string.h>

const char *p02_reg_name(p02_reg r)
{
    switch (r) {
    case P02_REG_NOT_REGISTERED: return "not registered, not searching";
    case P02_REG_SEARCHING:      return "searching";
    case P02_REG_DENIED:         return "registration denied";
    case P02_REG_HOME:           return "registered, home network";
    case P02_REG_ROAMING:        return "registered, roaming";
    case P02_REG_UNKNOWN:        break;
    }
    return "unknown";
}

p02_reg p02_reg_from_cereg(const char *line)
{
    char field[32];
    /* Field 1, not field 0. The first field is the unsolicited-result
     * setting the host chose, and reading it as the state is a mistake that
     * reports a healthy network whenever the host asked for notifications. */
    if (!at_field(line, 1, field, sizeof field))
        return P02_REG_UNKNOWN;
    switch (atoi(field)) {
    case 0: return P02_REG_NOT_REGISTERED;
    case 1: return P02_REG_HOME;
    case 2: return P02_REG_SEARCHING;
    case 3: return P02_REG_DENIED;
    case 5: return P02_REG_ROAMING;
    default: return P02_REG_UNKNOWN;
    }
}

static bool registered(p02_reg r)
{
    return r == P02_REG_HOME || r == P02_REG_ROAMING;
}

void p02_config_init(p02_config *c)
{
    if (c == NULL)
        return;
    memset(c, 0, sizeof *c);
    c->endpoint = "172.16.0.1";
    c->port = 5683;
    c->payload_hex = "50494E47";          /* PING, four bytes */
    c->max_payload_bytes = P02_PAYLOAD_MAX_BYTES;
    c->cycles = P02_CYCLES_DEFAULT;
    c->period_s = P02_PERIOD_DEFAULT_S;
    c->tau_bits = "00100011";
    c->active_bits = "00000101";
    c->band = 20;
}

void p02_session_init(p02_session *s)
{
    if (s == NULL)
        return;
    memset(s, 0, sizeof *s);
    s->last_rssi = 99;                    /* what +CSQ reports for unknown */
}

/* A command whose failure stops the lab. A timeout and a refusal are counted
 * apart because they are different faults: one is a module that is not
 * listening, the other is a module that understood and declined. */
static p02_status must(atchat *a, p02_session *s, const char *cmd)
{
    at_status r = atchat_command(a, cmd);
    if (r == AT_E_TIMEOUT) {
        s->at_timeouts++;
        return P02_E_AT;
    }
    if (r == AT_E_ERROR) {
        s->at_errors++;
        return P02_E_AT;
    }
    if (r != AT_OK)
        return P02_E_AT;
    return P02_OK;
}

p02_status p02_bringup(atchat *a, const p02_config *c, p02_session *s)
{
    if (a == NULL || c == NULL || s == NULL)
        return P02_E_ARG;

    p02_status st = p02_timer_parse(&s->tau_requested, P02_TIMER_TAU, c->tau_bits);
    if (st != P02_OK)
        return st;
    st = p02_timer_parse(&s->active_requested, P02_TIMER_ACTIVE, c->active_bits);
    if (st != P02_OK)
        return st;
    st = p02_payload_check(c->payload_hex, c->max_payload_bytes, NULL);
    if (st != P02_OK)
        return st;

    char cmd[AT_LINE_MAX];

    st = must(a, s, "AT");
    if (st != P02_OK)
        return st;
    st = must(a, s, "AT+CPIN?");
    if (st != P02_OK)
        return st;
    if (atchat_find(a, "+CPIN: READY") == NULL)
        return P02_E_AT;

    snprintf(cmd, sizeof cmd, "AT+CBAND=%u", (unsigned)c->band);
    st = must(a, s, cmd);
    if (st != P02_OK)
        return st;
    st = must(a, s, "AT+COPS=0");
    if (st != P02_OK)
        return st;

    /* The request. Two empty fields between the mode and the bitmaps, which
     * is the form AT+CPSMS takes: the GPRS timers of the second and third
     * arguments do not apply to this radio. */
    snprintf(cmd, sizeof cmd, "AT+CPSMS=1,,\"%s\",\"%s\"",
             c->tau_bits, c->active_bits);
    at_status r = atchat_command(a, cmd);
    if (r == AT_OK) {
        s->psm_requested = true;
    } else if (r == AT_E_ERROR) {
        /* Not fatal, and the chapter says what to do instead: cycle the
         * radio by hand between sends. The lab continues so that the rest of
         * the session can be observed. */
        s->at_errors++;
    } else {
        s->at_timeouts += (r == AT_E_TIMEOUT) ? 1u : 0u;
        return P02_E_AT;
    }

    /* The readback. Whatever comes back, it is what the module holds as its
     * request, and the grant is a separate thing that arrives from the
     * network. p02_session_print says so rather than leaving the reader to
     * infer it from a match. */
    if (atchat_command(a, "AT+CPSMS?") == AT_OK) {
        const char *line = atchat_find(a, "+CPSMS:");
        if (line != NULL) {
            char tau[32];
            char act[32];
            if (at_field(line, 2, tau, sizeof tau) &&
                at_field(line, 3, act, sizeof act)) {
                /* The fields arrive quoted. */
                char tb[32];
                char ab[32];
                snprintf(tb, sizeof tb, "%s", tau[0] == '"' ? tau + 1 : tau);
                snprintf(ab, sizeof ab, "%s", act[0] == '"' ? act + 1 : act);
                char *q = strchr(tb, '"');
                if (q != NULL)
                    *q = '\0';
                q = strchr(ab, '"');
                if (q != NULL)
                    *q = '\0';
                if (p02_timer_parse(&s->tau_readback, P02_TIMER_TAU, tb) == P02_OK &&
                    p02_timer_parse(&s->active_readback, P02_TIMER_ACTIVE, ab) == P02_OK) {
                    s->readback_parsed = true;
                    s->psm_readback_matches =
                        (strcmp(tb, c->tau_bits) == 0 && strcmp(ab, c->active_bits) == 0);
                }
            }
        }
    }
    return P02_OK;
}

p02_status p02_cycle(atchat *a, const p02_config *c, p02_session *s)
{
    if (a == NULL || c == NULL || s == NULL)
        return P02_E_ARG;

    s->cycles++;
    char cmd[AT_LINE_MAX];

    /* Registration first, and the indicator is settled here. */
    p02_status st = must(a, s, "AT+CEREG?");
    if (st != P02_OK)
        return st;
    const char *cereg = atchat_find(a, "+CEREG:");
    p02_reg reg = (cereg == NULL) ? P02_REG_UNKNOWN : p02_reg_from_cereg(cereg);
    s->last_reg = reg;
    const bool led_on = registered(reg);
    if (led_on)
        s->led_on_cycles++;
    else
        s->not_registered++;

    /* Signal strength is recorded and decides nothing. */
    if (must(a, s, "AT+CSQ") == P02_OK) {
        const char *csq = atchat_find(a, "+CSQ:");
        char field[32];
        if (csq != NULL && at_field(csq, 0, field, sizeof field))
            s->last_rssi = atoi(field);
    }

    if (!led_on) {
        /* Nothing is sent into a network that does not know this module.
         * The cycle is not an error: an unregistered wake is a result, and
         * the count of them is part of what the lab reports. */
        return P02_OK;
    }

    st = must(a, s, "AT+CSOC=1,2,1");
    if (st != P02_OK)
        return st;
    snprintf(cmd, sizeof cmd, "AT+CSOCON=0,%u,\"%s\"",
             (unsigned)c->port, c->endpoint);
    st = must(a, s, cmd);
    if (st != P02_OK)
        return st;

    uint32_t bytes = 0;
    st = p02_payload_check(c->payload_hex, c->max_payload_bytes, &bytes);
    if (st != P02_OK)
        return st;

    snprintf(cmd, sizeof cmd, "AT+CSOSEND=0,%lu,\"%s\"",
             (unsigned long)(strlen(c->payload_hex)), c->payload_hex);
    s->sends_attempted++;
    at_status r = atchat_command(a, cmd);
    if (r == AT_OK) {
        s->sends_accepted++;
    } else if (r == AT_E_ERROR) {
        s->at_errors++;
    } else if (r == AT_E_TIMEOUT) {
        s->at_timeouts++;
    } else {
        return P02_E_AT;
    }
    return P02_OK;
}

void p02_session_print(const p02_session *s, FILE *out)
{
    if (s == NULL || out == NULL)
        return;

    fprintf(out, "cycles %lu, sends attempted %lu, accepted %lu\n",
            (unsigned long)s->cycles, (unsigned long)s->sends_attempted,
            (unsigned long)s->sends_accepted);
    fprintf(out, "indicator on for %lu of %lu cycles, decided by the "
                 "registration state alone\n",
            (unsigned long)s->led_on_cycles, (unsigned long)s->cycles);
    fprintf(out, "last registration: %s, last signal %d%s\n",
            p02_reg_name(s->last_reg), s->last_rssi,
            s->last_rssi == 99 ? " (unknown, which is not zero)" : "");
    fprintf(out, "module refusals %lu, silences %lu\n",
            (unsigned long)s->at_errors, (unsigned long)s->at_timeouts);

    if (!s->psm_requested) {
        fprintf(out, "power saving was not accepted by the module, so the "
                     "radio is awake between sends unless it is cycled by "
                     "hand\n");
        return;
    }
    if (!s->readback_parsed) {
        fprintf(out, "power saving was requested and the readback could not "
                     "be parsed, so nothing is known about what the module "
                     "holds\n");
        return;
    }
    fprintf(out, "requested  ");
    p02_timer_print(&s->tau_requested, P02_TIMER_TAU, out);
    fprintf(out, "requested  ");
    p02_timer_print(&s->active_requested, P02_TIMER_ACTIVE, out);
    fprintf(out, "read back  ");
    p02_timer_print(&s->tau_readback, P02_TIMER_TAU, out);
    fprintf(out, "read back  ");
    p02_timer_print(&s->active_readback, P02_TIMER_ACTIVE, out);
    fprintf(out, "%s\n", s->psm_readback_matches
            ? "the module returned the octets it was given, which says what "
              "it asked for and not what it was granted"
            : "the module returned different octets from the ones requested, "
              "which is worth reading before anything else");
}
