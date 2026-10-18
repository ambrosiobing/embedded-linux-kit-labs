/* The lab book, and the judgement the chapter's acceptance test asks for.
 *
 * Steps 6 to 9 of the chapter are four separate claims and this file decides
 * the two that a program can reach:
 *
 *   step 7, the repeat   capture the first state again at the end of the run,
 *                        and if its median has moved, something on the bench
 *                        moved with it
 *   step 8, the compare  three states that overlay mean the DUT still has a
 *                        second power path, and the cause is the cable
 *
 * **Neither carries a threshold of mine**, which is this volume's first rule.
 * Two states are distinct when the quieter one's p95 sits below the busier
 * one's median: ninety five per cent of what the quiet state draws is below
 * what the busy state typically draws. The repeat holds when neither
 * capture's median has moved past the other's p95. Both statements are made
 * entirely out of measured quantities, so there is no number here for a
 * reader to argue with or for a bench to be tuned against.
 *
 * What this file does not decide is the first two acceptance items, which
 * ask whether deep-sleep is tens of microamps to low milliamps and whether
 * active radio is tens of milliamps. Those are judgements about a particular
 * board against its own datasheet, so the verdict reports the figures and
 * the order and says nothing about whether they are the right ones.
 *
 * The order is measured, not assumed. Nothing here believes that the state
 * named for deep sleep is the quietest one, because a bench where it is not
 * is exactly the bench this lab exists to catch.
 */
#define _POSIX_C_SOURCE 200809L

#include "p10.h"

#include "../../lib/linereader.h"

#include <string.h>

/* The scanf width below is written out, so it has to be kept in step with
 * the buffer by hand. This refuses to compile if that ever stops being true. */
_Static_assert(P10_NAME_MAX == 24, "the state-name scanf width says 23");

/* ------------------------------------------------------------------ book */

void p10_book_init(p10_book *b)
{
    if (b != NULL)
        memset(b, 0, sizeof *b);
}

static void copy_bounded(char *dst, size_t dstsz, const char *src)
{
    size_t i = 0;
    while (src != NULL && i + 1u < dstsz && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

p10_status p10_book_add(p10_book *b, const char *name, const p10_reduction *r,
                        const char *reason)
{
    if (b == NULL || name == NULL || r == NULL || name[0] == '\0')
        return P10_E_ARG;
    if (b->n >= P10_ENTRIES)
        return P10_E_FULL;

    p10_entry *e = &b->e[b->n];
    memset(e, 0, sizeof *e);
    copy_bounded(e->name, sizeof e->name, name);
    e->r = *r;
    copy_bounded(e->reason, sizeof e->reason, reason);
    b->n++;
    return P10_OK;
}

const p10_entry *p10_book_find(const p10_book *b, const char *name)
{
    if (b == NULL || name == NULL)
        return NULL;
    for (unsigned i = 0; i < b->n; i++) {
        if (strcmp(b->e[i].name, name) == 0)
            return &b->e[i];
    }
    return NULL;
}

p10_status p10_book_write(const p10_book *b, FILE *out)
{
    if (b == NULL || out == NULL)
        return P10_E_ARG;

    fprintf(out, "# P10 lab book. Currents in nanoamps, which is what the\n");
    fprintf(out, "# reductions are kept in; p10 report prints them readably.\n");
    fprintf(out, "# A bucket is the resolution a median and a p95 carry.\n");
    for (unsigned i = 0; i < b->n; i++) {
        const p10_entry *e = &b->e[i];
        fprintf(out,
                "state %s n=%lu refused=%lu lo=%lu hi=%lu median=%lu "
                "p95=%lu bucket=%lu\n",
                e->name, (unsigned long)e->r.n, (unsigned long)e->r.refused,
                (unsigned long)e->r.lo, (unsigned long)e->r.hi,
                (unsigned long)e->r.median, (unsigned long)e->r.p95,
                (unsigned long)e->r.bucket);
        if (e->reason[0] != '\0')
            fprintf(out, "why %s %s\n", e->name, e->reason);
    }
    return P10_OK;
}

p10_status p10_book_read(p10_book *b, const char *path)
{
    if (b == NULL || path == NULL)
        return P10_E_ARG;
    p10_book_init(b);

    linereader r;
    if (linereader_open(&r, path) != LINEREADER_OK)
        return P10_E_OPEN;

    const char *line;
    size_t len;
    p10_status rc = P10_OK;
    for (;;) {
        linereader_status s = linereader_line(&r, &line, &len);
        if (s == LINEREADER_EOF)
            break;
        if (s != LINEREADER_OK) {
            rc = P10_E_OPEN;
            break;
        }
        if (line[0] == '#')
            continue;

        if (strncmp(line, "state ", 6) == 0) {
            char name[P10_NAME_MAX];
            unsigned long n, refused, lo, hi, med, p95, bucket;
            int got = sscanf(line,
                             "state %23s n=%lu refused=%lu lo=%lu hi=%lu "
                             "median=%lu p95=%lu bucket=%lu",
                             name, &n, &refused, &lo, &hi, &med, &p95,
                             &bucket);
            if (got != 8) {
                rc = P10_E_FORMAT;
                break;
            }
            p10_reduction red;
            memset(&red, 0, sizeof red);
            red.n = (uint32_t)n;
            red.refused = (uint32_t)refused;
            red.lo = (uint32_t)lo;
            red.hi = (uint32_t)hi;
            red.median = (uint32_t)med;
            red.p95 = (uint32_t)p95;
            red.bucket = (uint32_t)bucket;
            rc = p10_book_add(b, name, &red, NULL);
            if (rc != P10_OK)
                break;
            continue;
        }

        if (strncmp(line, "why ", 4) == 0) {
            const char *p = line + 4;
            const char *sp = strchr(p, ' ');
            if (sp == NULL) {
                rc = P10_E_FORMAT;
                break;
            }
            char name[P10_NAME_MAX];
            size_t nlen = (size_t)(sp - p);
            if (nlen == 0 || nlen >= sizeof name) {
                rc = P10_E_FORMAT;
                break;
            }
            memcpy(name, p, nlen);
            name[nlen] = '\0';

            /* The reason attaches to the last entry of that name, so a
             * repeated state can carry its own. */
            bool placed = false;
            for (unsigned i = b->n; i > 0; i--) {
                if (strcmp(b->e[i - 1].name, name) == 0) {
                    copy_bounded(b->e[i - 1].reason,
                                 sizeof b->e[i - 1].reason, sp + 1);
                    placed = true;
                    break;
                }
            }
            if (!placed) {
                rc = P10_E_FORMAT;   /* a reason for a state nobody captured */
                break;
            }
            continue;
        }

        rc = P10_E_FORMAT;
        break;
    }

    linereader_close(&r);
    return rc;
}

/* ---------------------------------------------------------------- judge */

/* The first entry of each distinct name, which is what the separation test
 * compares. A repeated name is the step 7 capture and is judged on its own. */
static unsigned distinct_of(const p10_book *b, unsigned *idx)
{
    unsigned n = 0;
    for (unsigned i = 0; i < b->n; i++) {
        bool seen = false;
        for (unsigned j = 0; j < n; j++) {
            if (strcmp(b->e[idx[j]].name, b->e[i].name) == 0)
                seen = true;
        }
        /* Deliberately not filtered by sample count here. The judge checks
         * that every state has numbers, and a filter at this point would
         * make that check one that cannot fail. */
        if (!seen)
            idx[n++] = i;
    }
    return n;
}

void p10_judge(const p10_book *b, p10_verdict *v)
{
    if (v == NULL)
        return;
    memset(v, 0, sizeof *v);
    if (b == NULL || b->n == 0u)
        return;

    unsigned idx[P10_ENTRIES];
    unsigned n = distinct_of(b, idx);

    /* By rising median, which is measured rather than assumed. */
    for (unsigned i = 1; i < n; i++) {
        unsigned key = idx[i];
        unsigned j = i;
        while (j > 0 && b->e[idx[j - 1]].r.median > b->e[key].r.median) {
            idx[j] = idx[j - 1];
            j--;
        }
        idx[j] = key;
    }
    for (unsigned i = 0; i < n; i++)
        v->order[i] = idx[i];
    v->ordered = n;

    v->every_state_has_two_numbers = (n > 0);
    for (unsigned i = 0; i < n; i++) {
        if (b->e[idx[i]].r.n == 0u)
            v->every_state_has_two_numbers = false;
    }

    /* Step 8. Each state's p95 below the next one's median, with no number
     * of this program's own anywhere in the comparison. */
    v->separated = (n >= 2u);
    for (unsigned i = 0; i + 1u < n; i++) {
        const p10_entry *lo = &b->e[idx[i]];
        const p10_entry *hi = &b->e[idx[i + 1]];
        if (lo->r.p95 >= hi->r.median) {
            if (v->separated) {          /* name the first pair that failed */
                v->overlap_lo = idx[i];
                v->overlap_hi = idx[i + 1];
            }
            v->separated = false;
        }
    }

    /* Step 7. A state captured twice, and neither median past the other's
     * p95. */
    for (unsigned i = 0; i < b->n && !v->repeated; i++) {
        for (unsigned j = i + 1u; j < b->n; j++) {
            if (strcmp(b->e[i].name, b->e[j].name) == 0) {
                v->repeated = true;
                v->repeat_a = i;
                v->repeat_b = j;
                v->repeat_held = (b->e[j].r.median <= b->e[i].r.p95)
                                 && (b->e[i].r.median <= b->e[j].r.p95);
                break;
            }
        }
    }

    /* Step 9. The quietest state is the one whose figure would be quoted
     * later as a bare-module deep-sleep current, so it is the one that has
     * to carry the sentence saying why it is not. */
    if (n > 0)
        v->quietest_has_a_reason = (b->e[idx[0]].reason[0] != '\0');
}

bool p10_verdict_passed(const p10_verdict *v)
{
    if (v == NULL)
        return false;
    return v->ordered >= 3u
           && v->every_state_has_two_numbers
           && v->separated
           && v->repeated
           && v->repeat_held
           && v->quietest_has_a_reason;
}
