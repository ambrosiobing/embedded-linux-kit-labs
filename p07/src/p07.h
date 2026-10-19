/* P07 and P08: one program, because the rule that matters spans both.
 *
 * P01 and P09 share a program because their chapters say the difference is a
 * bus number. These two share one for a different reason: P08's acceptance
 * test contains an item about P07. Its numbers have to live in a different
 * log file, and a check that spans two labs has to be written once, in a
 * place that knows both.
 *
 * **The rule is P08's intent, stated there in one sentence:**
 *
 *   two streams, two keys, two log files. Collapsing them into one
 *   acceleration field is how a wearable-grade number ends up wearing an
 *   industrial label.
 *
 * And its data-flow section names the exact shape of the error: an analysis
 * "cannot find them already merged in a field called `acc` and assume the
 * merge was correct". So `acc` is refused by name, in every profile, and the
 * refusal quotes the chapter rather than paraphrasing it.
 *
 * Nothing here is invented. Every key name comes from the chapters: P07's
 * example record is `{"t":...,"rh":...,"p":...,"ax":...,"ay":...,"az":...}`
 * and P08's two files are `p08_lowg.jsonl` with `lowg` and
 * `p08_highg.jsonl` with `highg_peak`. This program reads a log written by
 * firmware that owns the format, and it says which keys it looked for.
 *
 * The one place a chapter states a rule without naming a key is the
 * barometer: "a pressure without the range it was measured in is a number
 * with two possible meanings". The ILPS22QS has two scales and the scale is
 * part of the record, but no chapter says what the scale key is called. So
 * that check is on the rule rather than on a name: a record carrying a
 * pressure must carry some key whose name contains "scale". Inventing the
 * name would have been inventing a format.
 */
#ifndef P07_H
#define P07_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ------------------------------------------------------------------ keys */

#define P07_KEYS_MAX 32u
#define P07_KEY_MAX  24u

typedef struct {
    char k[P07_KEYS_MAX][P07_KEY_MAX];
    unsigned n;
    bool overflowed;   /* more keys than this holds: a refusal, not a trim */
    bool truncated;    /* a key longer than this holds, likewise */
} p07_keyset;

/* The top-level keys of one JSON object, in the order they appear. Returns
 * false when the line is not a JSON object at all, which is a refusal and
 * never a partial answer. Nested objects and arrays are skipped whole, so a
 * key inside one is not mistaken for a key of the record. */
bool p07_keys_of(const char *line, p07_keyset *out);

bool p07_keys_equal(const p07_keyset *a, const p07_keyset *b);
bool p07_keys_have(const p07_keyset *s, const char *key);

/* -------------------------------------------------------------- profiles */

typedef struct {
    const char *name;
    const char *shield;
    const char *file;              /* what the chapter calls the log */
    const char *const *required;
    unsigned required_n;
    const char *const *forbidden;  /* keys whose presence is the defect */
    unsigned forbidden_n;
    bool pressure_needs_scale;     /* the ILPS22QS rule, P08 only */

    /* The acceptance items of this lab that a person checks with the shield
     * in front of them. It belongs to the profile rather than to the report,
     * because P07's are about breathing on a humidity part and P08's shield
     * has no humidity part at all. */
    const char *by_hand;
} p07_profile;

const p07_profile *p07_profile_of(const char *name);  /* NULL if unknown */
const p07_profile *p07_profiles(void);
unsigned p07_profile_count(void);

/* ------------------------------------------------------------------- log */

typedef enum {
    P07_OK = 0,
    P07_E_ARG,
    P07_E_OPEN,
    P07_E_EMPTY
} p07_status;

const char *p07_strstatus(p07_status s);

typedef struct {
    uint32_t lines;
    uint32_t records;       /* lines that were JSON objects */
    uint32_t refused;       /* lines that were not, counted and dropped */
    p07_keyset first;
    bool stable;
    uint32_t drift_line;    /* the line the keys first changed on */
    p07_keyset drifted;
    bool any_overflow;

    /* Checked on every record rather than only on the first, because a key
     * that appears half way through a capture is exactly the drift the
     * chapter warns about, and a forbidden key is a defect wherever it is. */
    uint32_t pressure_without_scale;
    uint32_t missing_required;
    char first_missing[P07_KEY_MAX];
    uint32_t forbidden_seen;
    char first_forbidden[P07_KEY_MAX];
} p07_log;

p07_status p07_read(const char *path, const p07_profile *p, p07_log *out);

/* --------------------------------------------------------------- verdict */

typedef struct {
    bool has_records;
    bool keys_stable;
    bool required_present;
    char missing[P07_KEY_MAX];     /* the first required key that was absent */
    bool nothing_forbidden;
    char present[P07_KEY_MAX];     /* the first forbidden key that was there */
    bool scale_with_pressure;
} p07_verdict;

void p07_judge(const p07_log *l, const p07_profile *p, p07_verdict *v);
bool p07_verdict_passed(const p07_verdict *v);

/* ------------------------------------------------------------------ pair
 *
 * P08's third acceptance item, which is about P07: the numbers live in a
 * different log file. Two ways that fails, and they are different mistakes.
 */
typedef enum {
    P07_PAIR_OK = 0,
    P07_PAIR_SAME_PATH,   /* one file given twice */
    P07_PAIR_SAME_KEYS,   /* two files, one schema: the same stream twice */
    P07_PAIR_UNREADABLE
} p07_pair;

const char *p07_strpair(p07_pair r);
p07_pair p07_compare(const char *a, const char *b);

/* ----------------------------------------------------------------- scenes */

typedef enum {
    P07_SCENE_IKS4A1 = 0,   /* P07's record, as its chapter prints it */
    P07_SCENE_LOWG,         /* P08's low-g file */
    P07_SCENE_HIGHG,        /* P08's high-g peak-hold file */
    P07_SCENE_DRIFT,        /* a key appears half way through the capture */
    P07_SCENE_COLLAPSED,    /* the two streams merged into a field called acc */
    P07_SCENE_HUBBED,       /* a record that silently lost keys */
    P07_SCENE_NOSCALE       /* a pressure with no range beside it */
} p07_scene_kind;

bool p07_scene_of(const char *name, p07_scene_kind *out);
p07_status p07_scene_write(p07_scene_kind kind, uint32_t records, FILE *out);

#endif /* P07_H */
