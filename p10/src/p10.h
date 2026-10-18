/* P10, the energy characterisation bench: three firmware states, two numbers
 * each, and the one comparison that says whether any of it means anything.
 *
 * The chapter's product is a small table: a median and a p95 for radio-off
 * idle, BLE advertise and deep-sleep. What makes the table worth anything is
 * not the arithmetic, it is the fourth acceptance item, which says that three
 * traces that overlay mean the DUT is still USB-powered and the cause is the
 * cable rather than the instrument. A bench that gets the wiring right and
 * the USB cable wrong produces traces that look like measurements, so this
 * program's real job is to refuse that run.
 *
 * Two decisions here follow from the volume's rules rather than from taste.
 *
 * **No default threshold anywhere**, so the separation test carries no
 * number of mine. Two states are distinguishable when the quieter one's p95
 * sits below the busier one's median, which is a statement entirely in
 * measured quantities: ninety five per cent of the quiet state's samples are
 * below what the busy state typically costs. Likewise the repeat of step 7
 * holds when neither capture's median has moved past the other's p95.
 *
 * **Nothing allocates on the data path.** Two hundred thousand samples do
 * not fit a sorted array in this volume's budget, so the quantiles come from
 * a fixed histogram in two passes: one for the range, one for the counts.
 * That makes every quantile exact only to a bucket, which is why every
 * reduction carries its bucket width and every printed figure is a bucket
 * midpoint. A quantile reported without its resolution is a number with a
 * decimal place it has not earned.
 *
 * The other two rules hold as everywhere: library code returns a status and
 * never exits, and nothing is repaired. A line that is not a sample is
 * counted and dropped, never guessed at.
 */
#ifndef P10_H
#define P10_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ------------------------------------------------------------------ units
 *
 * Nanoamps, as uint32_t, everywhere inside this program. One amp is
 * 1,000,000,000 nA and the type reaches 4,294,967,295, so the PPK2's whole
 * range from 200 nA to 1 A fits with room over. Integers throughout means
 * there is no floating point in this program at all, so there is no rounding
 * to argue about and the strict build has nothing to say about promotions.
 */
#define P10_NA_PER_UA 1000u
#define P10_NA_PER_MA 1000000u

/* Nanoamps printed as a figure a lab book would carry, which is microamps
 * below a milliamp and milliamps above it, three significant figures either
 * way. Needs 24 bytes. */
bool p10_format_current(char *buf, size_t n, uint32_t na);

/* ------------------------------------------------------------- quantiles */

#define P10_BUCKETS 4096u

typedef struct {
    uint32_t count[P10_BUCKETS];
    uint32_t n;
    uint32_t lo;      /* the smallest sample seen, inclusive */
    uint32_t hi;      /* the largest, inclusive */
} p10_hist;

void p10_hist_init(p10_hist *h, uint32_t lo, uint32_t hi);
void p10_hist_add(p10_hist *h, uint32_t na);

/* The bucket width in nanoamps, which is the resolution every quantile out
 * of this histogram carries. Never zero: a trace whose samples are all the
 * same value still has a width of one. */
uint32_t p10_hist_bucket(const p10_hist *h);

/* The quantile at `permille`, as the midpoint of the bucket it falls in.
 * 500 is the median and 950 is the p95. A histogram with no samples returns
 * zero, which a caller must tell apart from a measured zero by looking at n.
 */
uint32_t p10_hist_quantile(const p10_hist *h, unsigned permille);

#define P10_MEDIAN 500u
#define P10_P95    950u

/* ------------------------------------------------------------- reductions */

typedef struct {
    uint32_t n;         /* samples that were reduced */
    uint32_t refused;   /* lines that were not samples, counted not repaired */
    uint32_t lo;
    uint32_t hi;
    uint32_t median;
    uint32_t p95;
    uint32_t bucket;    /* the resolution the two quantiles above carry */
} p10_reduction;

typedef enum {
    P10_OK = 0,
    P10_E_ARG,
    P10_E_OPEN,      /* the capture would not open */
    P10_E_EMPTY,     /* it opened and held no sample at all */
    P10_E_LONGLINE,  /* a line too long for the reader, refused whole */
    P10_E_FORMAT,    /* a lab book line that is not a lab book line */
    P10_E_FULL       /* more entries than this book holds */
} p10_status;

const char *p10_strstatus(p10_status s);

/* Two passes over the file, which is why this takes a path rather than a
 * stream: the range has to be known before the counts can be placed. The
 * capture is a file on a disk, so reading it twice costs nothing anybody
 * will notice, and the alternative is holding it in memory.
 *
 * `column` is which comma-separated field carries the current, counting from
 * zero, because the vendor's export owns the format and this program only
 * says which column it was told to read. Microamps on the way in, which is
 * what the Power Profiler writes.
 */
p10_status p10_reduce_file(const char *path, unsigned column,
                           p10_reduction *out);

/* -------------------------------------------------------------- lab book */

#define P10_NAME_MAX   24
#define P10_REASON_MAX 160
#define P10_ENTRIES    8

typedef struct {
    char name[P10_NAME_MAX];
    p10_reduction r;
    char reason[P10_REASON_MAX];   /* empty when none was recorded */
} p10_entry;

typedef struct {
    p10_entry e[P10_ENTRIES];
    unsigned n;
} p10_book;

void p10_book_init(p10_book *b);
p10_status p10_book_add(p10_book *b, const char *name, const p10_reduction *r,
                        const char *reason);
p10_status p10_book_write(const p10_book *b, FILE *out);
p10_status p10_book_read(p10_book *b, const char *path);

/* The first entry carrying this name, or NULL. The repeat of step 7 is a
 * second entry with the same name, so this finds the original and
 * p10_book_repeat_of() finds the pair. */
const p10_entry *p10_book_find(const p10_book *b, const char *name);

/* ---------------------------------------------------------------- verdict
 *
 * The chapter's four acceptance items, as far as a program can reach them.
 * The first two are about magnitudes a reader must judge against the part,
 * so they are reported and not ruled on: this program says what was
 * measured and does not tell a reader whether tens of microamps is the right
 * answer for their board. The third and fourth are decidable here.
 */
typedef struct {
    unsigned order[P10_ENTRIES];  /* entry indices, by rising median */
    unsigned ordered;             /* how many are in `order` */

    bool separated;               /* every neighbouring pair is distinct */
    unsigned overlap_lo;          /* the pair that was not, when it was not */
    unsigned overlap_hi;

    bool repeated;                /* a state was captured a second time */
    bool repeat_held;             /* and neither median moved past the other's p95 */
    unsigned repeat_a;
    unsigned repeat_b;

    bool every_state_has_two_numbers;
    bool quietest_has_a_reason;   /* the honest sentence of step 9 */
} p10_verdict;

void p10_judge(const p10_book *b, p10_verdict *v);

/* The whole acceptance test this program can decide, in one answer. A false
 * here is a refusal and `p10_verdict` says which of the four it was. */
bool p10_verdict_passed(const p10_verdict *v);

/* ----------------------------------------------------------------- scenes
 *
 * Captures the bench did not take, so that the suite has data whose answers
 * can be worked out rather than observed.
 */
typedef enum {
    P10_SCENE_RAMP = 0,   /* an exact ramp, whose quantiles are arithmetic */
    P10_SCENE_SLEEP,      /* a quiet state with a little noise */
    P10_SCENE_IDLE,       /* a louder quiet state */
    P10_SCENE_BLE,        /* a baseline with advertising bursts over it */
    P10_SCENE_USB         /* what a DUT with its USB cable still on looks like */
} p10_scene_kind;

/* Writes `n` samples in microamps, one per line, with the header line the
 * vendor's export carries. Deterministic: the same kind and count give the
 * same file every time, because a suite that cannot reproduce its own input
 * is not testing the thing it names. */
p10_status p10_scene_write(p10_scene_kind kind, uint32_t n, FILE *out);

bool p10_scene_of(const char *name, p10_scene_kind *out);

#endif /* P10_H */
