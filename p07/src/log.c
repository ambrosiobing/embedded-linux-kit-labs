/* The profiles, the reader and the two judgements.
 *
 * Every key name below is quoted from a chapter. P07's record is printed in
 * its own data-flow section as
 * `{"t":24.1,"rh":41.2,"p":1013.2,"ax":0.02,"ay":0.01,"az":1.00}`, and P08's
 * two files are named there with their one key each. `acc` is forbidden in
 * all three because P08 names it as the shape of the error: an analysis
 * cannot find the two streams already merged in a field called `acc` and
 * assume the merge was correct.
 */
#define _POSIX_C_SOURCE 200809L

#include "p07.h"

#include "../../lib/linereader.h"

#include <string.h>

/* -------------------------------------------------------------- profiles */

static const char *const IKS4A1_REQUIRED[] = { "t", "rh", "p",
                                               "ax", "ay", "az" };
static const char *const IKS4A1_FORBIDDEN[] = { "acc", "lowg", "highg_peak" };

static const char *const LOWG_REQUIRED[] = { "lowg" };
static const char *const LOWG_FORBIDDEN[] = { "acc", "highg_peak" };

static const char *const HIGHG_REQUIRED[] = { "highg_peak" };
static const char *const HIGHG_FORBIDDEN[] = { "acc", "lowg" };

static const p07_profile PROFILES[] = {
    { "iks4a1", "X-NUCLEO-IKS4A1", "p07.jsonl",
      IKS4A1_REQUIRED, 6u, IKS4A1_FORBIDDEN, 3u, false },
    { "lowg", "X-NUCLEO-IKS5A1, low-g channel", "p08_lowg.jsonl",
      LOWG_REQUIRED, 1u, LOWG_FORBIDDEN, 2u, true },
    { "highg", "X-NUCLEO-IKS5A1, high-g peak-hold", "p08_highg.jsonl",
      HIGHG_REQUIRED, 1u, HIGHG_FORBIDDEN, 2u, true }
};

#define P07_PROFILES (sizeof PROFILES / sizeof PROFILES[0])

const p07_profile *p07_profiles(void)
{
    return PROFILES;
}

unsigned p07_profile_count(void)
{
    return (unsigned)P07_PROFILES;
}

const p07_profile *p07_profile_of(const char *name)
{
    if (name == NULL)
        return NULL;
    for (unsigned i = 0; i < P07_PROFILES; i++) {
        if (strcmp(PROFILES[i].name, name) == 0)
            return &PROFILES[i];
    }
    return NULL;
}

/* ------------------------------------------------------------------ read */

static void copy_key(char *dst, const char *src)
{
    size_t i = 0;
    while (i + 1u < P07_KEY_MAX && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/* The ILPS22QS rule, which the chapter states without naming the key: a
 * pressure without the range it was measured in is a number with two
 * possible meanings. So this looks for the rule rather than for a name. */
static bool has_scale(const p07_keyset *k)
{
    for (unsigned i = 0; i < k->n; i++) {
        if (strstr(k->k[i], "scale") != NULL)
            return true;
    }
    return false;
}

p07_status p07_read(const char *path, const p07_profile *p, p07_log *out)
{
    if (path == NULL || out == NULL)
        return P07_E_ARG;
    memset(out, 0, sizeof *out);
    out->stable = true;

    linereader r;
    if (linereader_open(&r, path) != LINEREADER_OK)
        return P07_E_OPEN;

    const char *line;
    size_t len;
    for (;;) {
        linereader_status s = linereader_line(&r, &line, &len);
        if (s == LINEREADER_EOF)
            break;
        if (s != LINEREADER_OK) {
            linereader_close(&r);
            return P07_E_OPEN;
        }
        out->lines++;

        p07_keyset keys;
        if (!p07_keys_of(line, &keys)) {
            /* Not a JSON object. Counted and dropped, never half read. */
            out->refused++;
            continue;
        }
        out->records++;
        if (keys.overflowed || keys.truncated)
            out->any_overflow = true;

        if (out->records == 1u) {
            out->first = keys;
        } else if (out->stable && !p07_keys_equal(&out->first, &keys)) {
            out->stable = false;
            out->drift_line = out->lines;
            out->drifted = keys;
        }

        if (p == NULL)
            continue;

        for (unsigned i = 0; i < p->required_n; i++) {
            if (!p07_keys_have(&keys, p->required[i])) {
                if (out->missing_required == 0u)
                    copy_key(out->first_missing, p->required[i]);
                out->missing_required++;
                break;      /* one missing key names the record once */
            }
        }
        for (unsigned i = 0; i < p->forbidden_n; i++) {
            if (p07_keys_have(&keys, p->forbidden[i])) {
                if (out->forbidden_seen == 0u)
                    copy_key(out->first_forbidden, p->forbidden[i]);
                out->forbidden_seen++;
                break;
            }
        }
        if (p->pressure_needs_scale && p07_keys_have(&keys, "p") &&
            !has_scale(&keys))
            out->pressure_without_scale++;
    }

    linereader_close(&r);
    if (out->lines == 0u)
        return P07_E_EMPTY;
    return P07_OK;
}

/* --------------------------------------------------------------- verdict */

void p07_judge(const p07_log *l, const p07_profile *p, p07_verdict *v)
{
    if (v == NULL)
        return;
    memset(v, 0, sizeof *v);
    if (l == NULL)
        return;

    v->has_records = (l->records > 0u);
    v->keys_stable = l->stable;
    v->required_present = (l->missing_required == 0u);
    copy_key(v->missing, l->first_missing);
    v->nothing_forbidden = (l->forbidden_seen == 0u);
    copy_key(v->present, l->first_forbidden);
    v->scale_with_pressure =
        (p == NULL || !p->pressure_needs_scale || l->pressure_without_scale == 0u);
}

bool p07_verdict_passed(const p07_verdict *v)
{
    if (v == NULL)
        return false;
    return v->has_records
           && v->keys_stable
           && v->required_present
           && v->nothing_forbidden
           && v->scale_with_pressure;
}

/* ------------------------------------------------------------------ pair */

const char *p07_strpair(p07_pair r)
{
    switch (r) {
    case P07_PAIR_OK:
        return "two files, two schemas";
    case P07_PAIR_SAME_PATH:
        return "the same file was given twice, so there is only one log here";
    case P07_PAIR_SAME_KEYS:
        return "two files carrying one schema, which is the same stream "
               "written down twice rather than two streams kept apart";
    case P07_PAIR_UNREADABLE:
        return "one of the two would not be read";
    }
    return "an unnamed result, which should not be reachable";
}

p07_pair p07_compare(const char *a, const char *b)
{
    if (a == NULL || b == NULL)
        return P07_PAIR_UNREADABLE;
    if (strcmp(a, b) == 0)
        return P07_PAIR_SAME_PATH;

    static p07_log la;
    static p07_log lb;
    if (p07_read(a, NULL, &la) != P07_OK || p07_read(b, NULL, &lb) != P07_OK)
        return P07_PAIR_UNREADABLE;
    if (la.records == 0u || lb.records == 0u)
        return P07_PAIR_UNREADABLE;

    if (p07_keys_equal(&la.first, &lb.first))
        return P07_PAIR_SAME_KEYS;
    return P07_PAIR_OK;
}

const char *p07_strstatus(p07_status s)
{
    switch (s) {
    case P07_OK:
        return "ok";
    case P07_E_ARG:
        return "a caller's mistake, which is a defect in this program";
    case P07_E_OPEN:
        return "the log would not open, or stopped part way";
    case P07_E_EMPTY:
        return "it opened and held nothing at all";
    }
    return "an unnamed status, which should not be reachable";
}
