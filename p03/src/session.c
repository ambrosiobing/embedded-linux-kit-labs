/* The cycle, with the ordering enforced by the function that would break it.
 *
 * Every command that transmits goes through transmit(), and transmit()
 * refuses while the program's model of the receiver says it is powered. The
 * model is updated only by the two commands that change it, so the model and
 * the module agree unless a command was lost, and a lost command is a
 * timeout that stops the cycle anyway.
 */

#include "p03.h"

#include <string.h>

void p03_config_init(p03_config *c)
{
    if (c == NULL)
        return;
    memset(c, 0, sizeof *c);
    c->broker = "test.mosquitto.org";
    c->port = 1883;
    c->client_id = "p03-catm";
    c->topic = "lab/p03/fix";
    c->cycles = P03_CYCLES_DEFAULT;
}

void p03_session_init(p03_session *s)
{
    if (s == NULL)
        return;
    memset(s, 0, sizeof *s);
}

static p03_status must(atchat *a, p03_session *s, const char *cmd)
{
    at_status r = atchat_command(a, cmd);
    if (r == AT_E_TIMEOUT) {
        s->at_timeouts++;
        return P03_E_AT;
    }
    if (r == AT_E_ERROR) {
        s->at_errors++;
        return P03_E_AT;
    }
    return (r == AT_OK) ? P03_OK : P03_E_AT;
}

/* The only way anything in this program transmits. Declared in the header
 * so that the suite can reach it, for the reason given there. */
p03_status p03_transmit(atchat *a, p03_session *s, const char *cmd)
{
    if (s->gnss_powered) {
        /* The rule, where it can actually hold. Not a warning and not a log
         * line: the command is not sent. */
        s->order_refusals++;
        return P03_E_ORDER;
    }
    return must(a, s, cmd);
}

static p03_status gnss_power(atchat *a, p03_session *s, bool on)
{
    p03_status st = must(a, s, on ? "AT+CGNSPWR=1" : "AT+CGNSPWR=0");
    if (st != P03_OK)
        return st;
    s->gnss_powered = on;
    return P03_OK;
}

p03_status p03_bringup(atchat *a, const p03_config *c, p03_session *s)
{
    if (a == NULL || c == NULL || s == NULL)
        return P03_E_ARG;

    p03_status st = must(a, s, "AT+CPIN?");
    if (st != P03_OK)
        return st;

    /* Lock the bearer before anything else, then confirm it rather than
     * assume it. The two commands are a request; +CPSI is the answer. */
    st = must(a, s, "AT+CNMP=38");
    if (st != P03_OK)
        return st;
    st = must(a, s, "AT+CMNB=1");
    if (st != P03_OK)
        return st;
    st = must(a, s, "AT+CPSI?");
    if (st != P03_OK)
        return st;

    const char *cpsi = atchat_find(a, "+CPSI:");
    s->bearer_catm = p03_bearer_is_catm(cpsi);
    if (!s->bearer_catm)
        return P03_E_BEARER;

    return must(a, s, "AT+CSQ");
}

p03_status p03_cycle(atchat *a, const p03_config *c, p03_session *s)
{
    if (a == NULL || c == NULL || s == NULL)
        return P03_E_ARG;

    s->cycles++;
    char cmd[AT_LINE_MAX];

    p03_status st = gnss_power(a, s, true);
    if (st != P03_OK)
        return st;

    st = must(a, s, "AT+CGNSINF");
    if (st != P03_OK) {
        /* Power the receiver down even when the poll failed. Leaving it on
         * would take the next cycle's transmit down with it, and the fault
         * would then look like an ordering defect rather than a lost
         * command. */
        gnss_power(a, s, false);
        return st;
    }

    p03_fix got;
    const bool have_new = p03_fix_parse(atchat_find(a, "+CGNSINF:"), &got);
    if (have_new) {
        s->last_fix = got;
        s->last_fix.cached = false;
        s->fixes_new++;
    } else if (s->last_fix.valid) {
        s->last_fix.cached = true;
        s->fixes_cached++;
    }

    /* Before any transmission, and before the branch that might skip one. */
    st = gnss_power(a, s, false);
    if (st != P03_OK)
        return st;

    if (!s->last_fix.valid) {
        /* No fix and nothing cached. The cycle is skipped rather than
         * filled in with the host clock, which is this lab's rule and the
         * reason the chapter exists. */
        s->skipped++;
        s->indicator = false;
        return P03_OK;
    }

    char payload[128];
    if (p03_payload(&s->last_fix, payload, sizeof payload) == 0) {
        s->skipped++;
        return P03_OK;
    }

    snprintf(cmd, sizeof cmd, "AT+SMCONF=\"URL\",\"%s\",%u",
             c->broker, (unsigned)c->port);
    st = p03_transmit(a, s, cmd);
    if (st != P03_OK)
        return st;
    snprintf(cmd, sizeof cmd, "AT+SMCONF=\"CLIENTID\",\"%s\"", c->client_id);
    st = p03_transmit(a, s, cmd);
    if (st != P03_OK)
        return st;
    st = p03_transmit(a, s, "AT+SMCONN");
    if (st != P03_OK)
        return st;

    snprintf(cmd, sizeof cmd, "AT+SMPUB=\"%s\",%lu,0,1",
             c->topic, (unsigned long)strlen(payload));
    st = p03_transmit(a, s, cmd);
    if (st != P03_OK)
        return st;

    at_status r = atchat_payload(a, payload);
    if (r == AT_E_TIMEOUT) {
        s->at_timeouts++;
        return P03_E_AT;
    }
    if (r == AT_E_ERROR) {
        s->at_errors++;
        return P03_E_AT;
    }
    if (r != AT_OK)
        return P03_E_AT;

    s->published++;
    s->indicator = s->bearer_catm;
    snprintf(s->last_payload, sizeof s->last_payload, "%s", payload);
    return P03_OK;
}

void p03_session_print(const p03_session *s, FILE *out)
{
    if (s == NULL || out == NULL)
        return;

    fprintf(out, "cycles %lu, published %lu, skipped %lu\n",
            (unsigned long)s->cycles, (unsigned long)s->published,
            (unsigned long)s->skipped);
    fprintf(out, "fixes: %lu new, %lu republished with the stamp they came with\n",
            (unsigned long)s->fixes_new, (unsigned long)s->fixes_cached);
    fprintf(out, "bearer %s, indicator %s\n",
            s->bearer_catm ? "Cat-M, as this lab requires"
                           : "NOT Cat-M, which is what this lab exists to notice",
            s->indicator ? "on" : "off");
    fprintf(out, "module refusals %lu, silences %lu, ordering refusals %lu\n",
            (unsigned long)s->at_errors, (unsigned long)s->at_timeouts,
            (unsigned long)s->order_refusals);
    if (s->last_payload[0] != '\0')
        fprintf(out, "last payload: %s%s\n", s->last_payload,
                s->last_fix.cached ? "   (a repeat, and visibly one)" : "");
    if (s->gnss_powered)
        fprintf(out, "the receiver is still powered, which is not how a cycle "
                     "should end\n");
}
