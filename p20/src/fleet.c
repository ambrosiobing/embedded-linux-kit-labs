/* Reading the artifact, and judging it.
 *
 * The chapter's sweep emits each host's answers back to back with no
 * separator, and the first line of every block is that host's `hostname`.
 * So a line that is exactly one of the four fleet names opens a block, and
 * everything after it belongs to that host until the next such line. No
 * format is invented here: this reads what the chapter's own commands print.
 *
 * The one thing the sweep was asked to add is a label on the HAT line. The
 * chapter already insisted that an empty header report `none` explicitly,
 * because a blank reads as a broken script rather than as an empty header;
 * an unlabelled product string has the same problem one step further on,
 * since it is indistinguishable from any other line of output. `hat: none`
 * and `hat: SIM7600E-H` are both reports. A missing line is not.
 */
#define _POSIX_C_SOURCE 200809L

#include "p20.h"

#include "../../lib/linereader.h"

#include <string.h>

/* ------------------------------------------------------------------ fleet */

static const p20_host FLEET[P20_HOSTS] = {
    { "pi4",   true,  1u, "Raspberry Pi 4" },
    { "pi3bp", true,  1u, "Raspberry Pi 3B+" },
    { "pi3",   true,  1u, "Raspberry Pi 3" },
    /* vcgencmd is a Raspberry Pi tool and this is not a Raspberry Pi, and
     * its header bus is 0 rather than 1, which P09 established on the
     * bench. Both absences are honest and neither is a fault. */
    { "air",   false, 0u, "NanoPi NEO Air" }
};

const p20_host *p20_fleet(void)
{
    return FLEET;
}

const p20_host *p20_host_of(const char *name)
{
    if (name == NULL)
        return NULL;
    for (unsigned i = 0; i < P20_HOSTS; i++) {
        if (strcmp(FLEET[i].name, name) == 0)
            return &FLEET[i];
    }
    return NULL;
}

static int index_of(const char *name)
{
    for (unsigned i = 0; i < P20_HOSTS; i++) {
        if (strcmp(FLEET[i].name, name) == 0)
            return (int)i;
    }
    return -1;
}

/* ----------------------------------------------------------------- lines */

static void copy_bounded(char *dst, size_t dstsz, const char *src, size_t n)
{
    size_t i = 0;
    while (i + 1u < dstsz && i < n && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static const char *skip_spaces(const char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    return s;
}

/* An ISO-8601 stamp of the kind `date -Is` writes: four digits, a hyphen,
 * two digits, a hyphen. Specific enough that no other line in this sweep
 * reaches it. */
static bool looks_dated(const char *s, size_t len)
{
    if (len < 10u)
        return false;
    for (unsigned i = 0; i < 4u; i++) {
        if (s[i] < '0' || s[i] > '9')
            return false;
    }
    return s[4] == '-' && s[7] == '-';
}

/* A bare token: no space and no colon. The chapter's `hostname` prints one,
 * and so does nothing else in the sweep once the lines that have their own
 * shape have been taken first. */
static bool bare_token(const char *s, size_t len)
{
    if (len == 0u || len >= P20_NAME_MAX)
        return false;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == ' ' || s[i] == '\t' || s[i] == ':' || s[i] == '=')
            return false;
    }
    return true;
}

p20_status p20_read(const char *path, p20_sweep *out)
{
    if (path == NULL || out == NULL)
        return P20_E_ARG;
    memset(out, 0, sizeof *out);

    linereader r;
    if (linereader_open(&r, path) != LINEREADER_OK)
        return P20_E_OPEN;

    int cur = -1;
    const char *line;
    size_t len;
    for (;;) {
        linereader_status s = linereader_line(&r, &line, &len);
        if (s == LINEREADER_EOF)
            break;
        if (s != LINEREADER_OK) {
            linereader_close(&r);
            return P20_E_OPEN;
        }
        out->lines++;

        if (line[0] == '#')
            continue;

        /* The shaped lines first, so that a bare token test never sees
         * something like temp=46.2'C and calls it a host name. */
        if (strncmp(line, "hat:", 4) == 0) {
            if (cur >= 0) {
                p20_block *b = &out->b[cur];
                b->hat_reported = true;
                const char *v = skip_spaces(line + 4);
                if (strcmp(v, "none") == 0) {
                    b->hat_named = false;
                    b->hat[0] = '\0';
                } else if (*v != '\0') {
                    b->hat_named = true;
                    /* Counted here and not above, because exclusivity is
                     * about named boards. Two `hat: none` lines are two
                     * reports of an empty header, which is untidy and is
                     * not two HATs on one host. */
                    b->hats_named++;
                    copy_bounded(b->hat, sizeof b->hat, v, strlen(v));
                } else {
                    /* `hat:` with nothing after it is the pitfall itself:
                     * a blank where a name or `none` belongs. */
                    b->hat_reported = false;
                }
            }
            continue;
        }
        if (strstr(line, "temp=") != NULL) {
            if (cur >= 0)
                out->b[cur].temp_seen = true;
            continue;
        }
        if (strncmp(line, "Mem:", 4) == 0) {
            if (cur >= 0)
                out->b[cur].ram_seen = true;
            continue;
        }
        if (looks_dated(line, len)) {
            out->dated = true;
            continue;
        }

        if (bare_token(line, len)) {
            char name[P20_NAME_MAX];
            copy_bounded(name, sizeof name, line, len);
            int k = index_of(name);
            if (k >= 0) {
                cur = k;
                out->b[k].seen = true;
            } else {
                /* A fifth name in the loop, which the chapter's own pitfall
                 * calls a host this book has not brought up. */
                if (out->strangers == 0u)
                    copy_bounded(out->stranger, sizeof out->stranger, line, len);
                out->strangers++;
                cur = -1;
            }
            continue;
        }

        if (cur < 0)
            out->before_any_host++;
    }

    linereader_close(&r);
    if (out->lines == 0u)
        return P20_E_EMPTY;
    return P20_OK;
}

/* --------------------------------------------------------------- verdict */

void p20_judge(const p20_sweep *s, p20_verdict *v)
{
    if (v == NULL)
        return;
    memset(v, 0, sizeof *v);
    if (s == NULL)
        return;

    v->every_hat_reported = true;
    v->exclusive = true;

    for (unsigned i = 0; i < P20_HOSTS; i++) {
        const p20_block *b = &s->b[i];
        if (!b->seen)
            continue;
        v->present++;

        if (!b->hat_reported) {
            if (v->every_hat_reported)
                v->first_silent_hat = i;
            v->every_hat_reported = false;
        }
        if (b->hats_named > 1u) {
            if (v->exclusive)
                v->doubled = i;
            v->exclusive = false;
        }

        /* The same missing line, counted as two different things. */
        if (!b->temp_seen) {
            if (FLEET[i].answers_temp) {
                if (v->gaps == 0u)
                    v->first_gap_host = i;
                v->gaps++;
            } else {
                v->honest_absences++;
            }
        }
        if (!b->ram_seen) {
            if (v->gaps == 0u)
                v->first_gap_host = i;
            v->gaps++;      /* every host in this fleet runs free -h */
        }
    }

    v->four_hosts = (v->present == P20_HOSTS);
    v->no_stranger = (s->strangers == 0u);
    v->dated = s->dated;
}

bool p20_verdict_passed(const p20_verdict *v)
{
    if (v == NULL)
        return false;
    return v->four_hosts
           && v->every_hat_reported
           && v->exclusive
           && v->no_stranger
           && v->dated
           && v->gaps == 0u;
}

const char *p20_strstatus(p20_status s)
{
    switch (s) {
    case P20_OK:
        return "ok";
    case P20_E_ARG:
        return "a caller's mistake, which is a defect in this program";
    case P20_E_OPEN:
        return "the fleet file would not open, or stopped part way";
    case P20_E_EMPTY:
        return "it opened and held nothing at all";
    }
    return "an unnamed status, which should not be reachable";
}
