/* Two hundred thousand samples reduced to two numbers, without holding them.
 *
 * The quantiles come from a fixed histogram in two passes: one to learn the
 * range, one to fill the counts. That is why a capture arrives here as a
 * path and never on standard input, which the other labs in this volume do
 * accept: a stream can be read once and a second pass needs a file. The
 * limitation is stated rather than worked around, because the alternative is
 * holding the whole capture in memory and this volume does not allocate on
 * the data path.
 *
 * What the histogram costs is resolution. Every quantile out of it is the
 * midpoint of a bucket, so every reduction carries its bucket width and
 * every printed figure can be read with it. A quantile printed without its
 * resolution is a number with a decimal place it has not earned.
 *
 * Nothing is repaired. A line that is not a sample is counted and dropped,
 * and the count is printed with the result. One consequence is worth stating
 * plainly: a negative reading is refused, not clamped to zero. The PPK2 can
 * report small negatives in the noise around a true zero, and clamping them
 * would move a median upward by an amount nobody could see afterwards. None
 * of the three states this lab measures sits at a true zero, so a capture
 * where this matters is a capture whose refused count is large, and that
 * number is on every line of output.
 */
#define _POSIX_C_SOURCE 200809L

#include "p10.h"

#include "../../lib/linereader.h"
#include "../../lib/thousandths.h"

#include <string.h>

/* ---------------------------------------------------------------- format */

bool p10_format_current(char *buf, size_t n, uint32_t na)
{
    if (na >= P10_NA_PER_MA)
        return thousandths_format(buf, n, na / P10_NA_PER_UA, "mA");
    return thousandths_format(buf, n, na, "uA");
}

/* ------------------------------------------------------------- histogram */

void p10_hist_init(p10_hist *h, uint32_t lo, uint32_t hi)
{
    if (h == NULL)
        return;
    memset(h->count, 0, sizeof h->count);
    h->n = 0;
    h->lo = lo;
    h->hi = (hi >= lo) ? hi : lo;
}

uint32_t p10_hist_bucket(const p10_hist *h)
{
    if (h == NULL)
        return 1u;
    uint64_t span = (uint64_t)h->hi - (uint64_t)h->lo + 1u;
    uint64_t w = (span + (uint64_t)P10_BUCKETS - 1u) / (uint64_t)P10_BUCKETS;
    return (w == 0u) ? 1u : (uint32_t)w;
}

static unsigned bucket_of(const p10_hist *h, uint32_t na)
{
    if (na <= h->lo)
        return 0u;
    uint64_t k = ((uint64_t)na - (uint64_t)h->lo) / (uint64_t)p10_hist_bucket(h);
    return (k >= (uint64_t)P10_BUCKETS) ? P10_BUCKETS - 1u : (unsigned)k;
}

void p10_hist_add(p10_hist *h, uint32_t na)
{
    if (h == NULL)
        return;
    h->count[bucket_of(h, na)]++;
    h->n++;
}

uint32_t p10_hist_quantile(const p10_hist *h, unsigned permille)
{
    if (h == NULL || h->n == 0u)
        return 0u;

    /* The rank this quantile asks for, rounded up, so the median of an even
     * count is the upper of the two middle samples rather than an average
     * of two buckets that nothing measured. */
    uint64_t target = ((uint64_t)h->n * (uint64_t)permille + 999u) / 1000u;
    if (target == 0u)
        target = 1u;
    if (target > (uint64_t)h->n)
        target = (uint64_t)h->n;

    uint32_t w = p10_hist_bucket(h);
    uint64_t seen = 0;
    for (unsigned k = 0; k < P10_BUCKETS; k++) {
        seen += (uint64_t)h->count[k];
        if (seen >= target) {
            uint64_t mid = (uint64_t)h->lo + (uint64_t)k * (uint64_t)w
                           + (uint64_t)(w / 2u);
            if (mid > (uint64_t)h->hi)
                mid = (uint64_t)h->hi;
            return (uint32_t)mid;
        }
    }
    return h->hi;
}

/* ----------------------------------------------------------------- parse */

/* One field of a comma-separated line, counting from zero. Returns NULL when
 * the line has no such field. */
static const char *field_of(const char *line, unsigned column, size_t *len)
{
    const char *p = line;
    for (unsigned i = 0; i < column; i++) {
        const char *comma = strchr(p, ',');
        if (comma == NULL)
            return NULL;
        p = comma + 1;
    }
    const char *comma = strchr(p, ',');
    *len = (comma != NULL) ? (size_t)(comma - p) : strlen(p);
    return p;
}

/* Microamps with an optional fractional part, to nanoamps. Refuses anything
 * that is not wholly a number, including a negative one, and refuses a value
 * that would not fit the range the PPK2 can report. */
static bool parse_ua(const char *s, size_t len, uint32_t *na)
{
    size_t i = 0;
    while (i < len && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r'))
        i++;
    if (i >= len || s[i] == '-')
        return false;

    uint64_t whole = 0;
    size_t digits = 0;
    while (i < len && s[i] >= '0' && s[i] <= '9') {
        whole = whole * 10u + (uint64_t)(s[i] - '0');
        if (whole > 5000000u)          /* 5 A in microamps, past the instrument */
            return false;
        i++;
        digits++;
    }
    if (digits == 0)
        return false;

    uint64_t frac = 0;                  /* thousandths of a microamp */
    if (i < len && s[i] == '.') {
        i++;
        uint64_t scale = 100u;
        while (i < len && s[i] >= '0' && s[i] <= '9') {
            if (scale > 0u) {
                frac += (uint64_t)(s[i] - '0') * scale;
                scale /= 10u;
            }
            i++;                        /* digits past the third are dropped */
        }
    }

    while (i < len && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r'))
        i++;
    if (i != len)
        return false;                   /* trailing rubbish is not a number */

    uint64_t total = whole * (uint64_t)P10_NA_PER_UA + frac;
    if (total > 0xFFFFFFFFu)
        return false;
    *na = (uint32_t)total;
    return true;
}

/* ----------------------------------------------------------------- passes */

typedef struct {
    bool any;
    uint32_t lo;
    uint32_t hi;
    uint32_t kept;
    uint32_t refused;
} p10_range;

static p10_status walk(const char *path, unsigned column, p10_range *range,
                       p10_hist *h)
{
    linereader r;
    if (linereader_open(&r, path) != LINEREADER_OK)
        return P10_E_OPEN;

    const char *line;
    size_t len;
    for (;;) {
        linereader_status s = linereader_line(&r, &line, &len);
        if (s == LINEREADER_EOF)
            break;
        if (s != LINEREADER_OK) {
            linereader_close(&r);
            return P10_E_OPEN;
        }
        if (len == 0 || line[0] == '#')   /* a comment is not a refusal */
            continue;

        size_t flen = 0;
        const char *f = field_of(line, column, &flen);
        uint32_t na = 0;
        if (f == NULL || !parse_ua(f, flen, &na)) {
            if (range != NULL)
                range->refused++;
            continue;
        }

        if (range != NULL) {
            if (!range->any || na < range->lo)
                range->lo = na;
            if (!range->any || na > range->hi)
                range->hi = na;
            range->any = true;
            range->kept++;
        }
        if (h != NULL)
            p10_hist_add(h, na);
    }

    if (r.long_lines > 0 && range != NULL)
        range->refused += r.long_lines;
    linereader_close(&r);
    return P10_OK;
}

p10_status p10_reduce_file(const char *path, unsigned column,
                           p10_reduction *out)
{
    if (path == NULL || out == NULL)
        return P10_E_ARG;
    if (strcmp(path, "-") == 0)
        return P10_E_ARG;        /* two passes need a file, not a stream */

    memset(out, 0, sizeof *out);

    p10_range range;
    memset(&range, 0, sizeof range);
    p10_status s = walk(path, column, &range, NULL);
    if (s != P10_OK)
        return s;
    if (!range.any) {
        out->refused = range.refused;
        return P10_E_EMPTY;
    }

    static p10_hist h;           /* 16 kB, and not on the stack */
    p10_hist_init(&h, range.lo, range.hi);
    s = walk(path, column, NULL, &h);
    if (s != P10_OK)
        return s;

    out->n = h.n;
    out->refused = range.refused;
    out->lo = range.lo;
    out->hi = range.hi;
    out->median = p10_hist_quantile(&h, P10_MEDIAN);
    out->p95 = p10_hist_quantile(&h, P10_P95);
    out->bucket = p10_hist_bucket(&h);
    return P10_OK;
}

const char *p10_strstatus(p10_status s)
{
    switch (s) {
    case P10_OK:
        return "ok";
    case P10_E_ARG:
        return "a caller's mistake, which includes a capture on standard "
               "input: two passes need a file";
    case P10_E_OPEN:
        return "the capture would not open, or stopped part way";
    case P10_E_EMPTY:
        return "it opened and held no sample at all";
    case P10_E_LONGLINE:
        return "a line was longer than the reader's buffer and was refused "
               "whole";
    case P10_E_FORMAT:
        return "a lab book line that is not a lab book line";
    case P10_E_FULL:
        return "more entries than this book holds";
    }
    return "an unnamed status, which should not be reachable";
}
