/* The suite. No shield, no Nucleo, no USB cable, and no capture any bench
 * took.
 *
 * Five groups:
 *
 *   keys      the parser, and the three ways a naive one goes wrong
 *   profiles  whether the three profiles carry what the chapters say
 *   logs      whether each bad capture is refused, and for its own reason
 *   pair      P08's third acceptance item, which is an item about P07
 *   status    whether every status and every pair result says something once
 *
 * Four of the five bad captures are wrong in exactly one way and the suite
 * asserts that the other checks still pass on each. **The fifth is wrong in
 * two ways and that is not a flaw in the scene**: collapsing the two streams
 * into a field called `acc` both introduces a forbidden key and removes a
 * required one, because that is what collapsing is. Asserting one of those
 * and not the other would describe the defect as half of itself.
 */

#include "../src/p07.h"

#include <stdio.h>
#include <string.h>

static int g_checks = 0;
static int g_failed = 0;

static void ok(bool cond, const char *what)
{
    g_checks++;
    if (cond) {
        printf("ok   %s\n", what);
    } else {
        g_failed++;
        printf("FAIL %s\n", what);
    }
}

static const char *TMP_A = "test_p07_a.jsonl";
static const char *TMP_B = "test_p07_b.jsonl";

static bool write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL)
        return false;
    fputs(text, f);
    fclose(f);
    return true;
}

static bool scene_file(p07_scene_kind k, uint32_t n, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL)
        return false;
    p07_status s = p07_scene_write(k, n, f);
    fclose(f);
    return s == P07_OK;
}

static bool run_scene(p07_scene_kind k, uint32_t n, const char *profile,
                      p07_log *l, p07_verdict *v)
{
    if (!scene_file(k, n, TMP_A))
        return false;
    const p07_profile *p = p07_profile_of(profile);
    if (p == NULL)
        return false;
    if (p07_read(TMP_A, p, l) != P07_OK)
        return false;
    p07_judge(l, p, v);
    return true;
}

/* ----------------------------------------------------------------- keys */

static void test_keys(void)
{
    printf("\n-- the parser, and the three ways a naive one goes wrong\n");

    p07_keyset k;

    ok(p07_keys_of("{\"t\":24.1,\"rh\":41.2,\"p\":1013.2,"
                   "\"ax\":0.02,\"ay\":0.01,\"az\":1.00}", &k) && k.n == 6u,
       "P07's own example record, printed in its chapter, has six keys");
    ok(p07_keys_have(&k, "t") && p07_keys_have(&k, "rh") &&
       p07_keys_have(&k, "az"),
       "and they are the ones the chapter prints");
    ok(!p07_keys_have(&k, "acc"), "and none of them is acc");

    /* One: a brace inside a string is not a brace. */
    ok(p07_keys_of("{\"a\":\"x{y}z\",\"b\":1}", &k) && k.n == 2u &&
       p07_keys_have(&k, "a") && p07_keys_have(&k, "b"),
       "a brace inside a string value does not end the object");

    /* Two: a key inside a nested object is not a key of the record. */
    ok(p07_keys_of("{\"a\":{\"b\":1},\"c\":2}", &k) && k.n == 2u &&
       p07_keys_have(&k, "a") && p07_keys_have(&k, "c") &&
       !p07_keys_have(&k, "b"),
       "a nested object is skipped whole, so its key is not the record's");
    ok(p07_keys_of("{\"a\":[{\"b\":1},{\"d\":2}],\"c\":3}", &k) && k.n == 2u &&
       !p07_keys_have(&k, "b") && !p07_keys_have(&k, "d"),
       "and so is an array of objects");

    /* Three: a line that is not an object is refused whole. */
    ok(!p07_keys_of("hello", &k), "a bare word is not a record");
    ok(!p07_keys_of("[1,2,3]", &k), "nor is an array");
    ok(!p07_keys_of("{\"a\":1", &k), "nor an object that never closes");
    ok(!p07_keys_of("{\"a\"}", &k), "nor a key with no value");
    ok(!p07_keys_of("{\"a\":1} and then some", &k),
       "nor an object with anything after it, because one line is one "
       "record");
    ok(!p07_keys_of("{\"a\":\"never closed}", &k),
       "nor one whose string never closes");
    ok(!p07_keys_of(NULL, &k), "and no line at all is not a record");

    ok(p07_keys_of("{}", &k) && k.n == 0u,
       "an empty object is a record with no keys, which is not the same as "
       "not being a record");

    ok(p07_keys_of("{\"a\\\"b\":1}", &k) && k.n == 1u &&
       p07_keys_have(&k, "a\"b"),
       "an escaped quote inside a key is part of the key");

    ok(p07_keys_of("  {\"a\":1}  ", &k) && k.n == 1u,
       "space around the record is not part of it");

    printf("\n   two key sets, compared as sets\n");
    p07_keyset a;
    p07_keyset b;
    ok(p07_keys_of("{\"x\":1,\"y\":2}", &a) &&
       p07_keys_of("{\"y\":9,\"x\":8}", &b) && p07_keys_equal(&a, &b),
       "the same keys in a different order are the same schema, because a "
       "reader parses by name");
    ok(p07_keys_of("{\"x\":1,\"y\":2,\"z\":3}", &b) && !p07_keys_equal(&a, &b),
       "and one more key is not");
}

/* ------------------------------------------------------------- profiles */

static void test_profiles(void)
{
    printf("\n-- three profiles, every key quoted from a chapter\n");

    const p07_profile *p = p07_profile_of("iks4a1");
    ok(p != NULL && p->required_n == 6u,
       "iks4a1 wants the six keys P07's example record carries");
    ok(p != NULL && strcmp(p->file, "p07.jsonl") == 0,
       "and its file is the one the chapter names");

    const p07_profile *lo = p07_profile_of("lowg");
    const p07_profile *hi = p07_profile_of("highg");
    ok(lo != NULL && strcmp(lo->file, "p08_lowg.jsonl") == 0 &&
       strcmp(lo->required[0], "lowg") == 0,
       "lowg wants the lowg key in p08_lowg.jsonl");
    ok(hi != NULL && strcmp(hi->file, "p08_highg.jsonl") == 0 &&
       strcmp(hi->required[0], "highg_peak") == 0,
       "highg wants the highg_peak key in p08_highg.jsonl");

    ok(lo != NULL && hi != NULL && lo->pressure_needs_scale &&
       hi->pressure_needs_scale && p != NULL && !p->pressure_needs_scale,
       "a pressure needs its range on P08's shield, which has two scales, "
       "and not on P07's, which has one");

    /* `acc` is the error P08 names, so it is refused in all three. */
    unsigned refusing_acc = 0;
    const p07_profile *all = p07_profiles();
    for (unsigned i = 0; i < p07_profile_count(); i++) {
        for (unsigned k = 0; k < all[i].forbidden_n; k++) {
            if (strcmp(all[i].forbidden[k], "acc") == 0)
                refusing_acc++;
        }
    }
    ok(p07_profile_count() == 3u, "there are three profiles");
    ok(refusing_acc == 3u,
       "and all three refuse a key called acc, which is the field P08 names "
       "as the shape of the error");

    ok(p07_profile_of("iks5a1") == NULL,
       "a profile nobody defined is absent rather than a default");
    ok(p07_profile_of(NULL) == NULL, "and so is no name at all");
}

/* ----------------------------------------------------------------- logs */

static bool others_hold(const p07_verdict *v, const char *except)
{
    bool stable = v->keys_stable || strcmp(except, "stable") == 0;
    bool req = v->required_present || strcmp(except, "required") == 0;
    bool forb = v->nothing_forbidden || strcmp(except, "forbidden") == 0;
    bool scale = v->scale_with_pressure || strcmp(except, "scale") == 0;
    return v->has_records && stable && req && forb && scale;
}

static void test_logs(void)
{
    printf("\n-- the captures, and what each refusal is about\n");

    p07_log l;
    p07_verdict v;

    ok(run_scene(P07_SCENE_IKS4A1, 20u, "iks4a1", &l, &v) &&
       p07_verdict_passed(&v),
       "P07's capture is what its chapter says it is");
    ok(l.records == 20u && l.refused == 0u, "twenty records, none refused");

    ok(run_scene(P07_SCENE_LOWG, 20u, "lowg", &l, &v) &&
       p07_verdict_passed(&v), "P08's low-g capture passes");
    ok(run_scene(P07_SCENE_HIGHG, 20u, "highg", &l, &v) &&
       p07_verdict_passed(&v), "and so does its high-g peak-hold capture");

    /* Drift: the keys change half way, and nothing else is wrong. With ten
     * records the generator switches at i*2 >= 10, so record six is the
     * first of the new shape and that is line six. */
    ok(run_scene(P07_SCENE_DRIFT, 10u, "iks4a1", &l, &v),
       "a capture whose keys change half way through");
    ok(!v.keys_stable && !p07_verdict_passed(&v),
       "is refused, because a file whose schema drifts is a file no parser "
       "reads end to end");
    ok(l.drift_line == 6u,
       "and the drift is named at line 6, which is where the sixth record "
       "is");
    ok(others_hold(&v, "stable"), "and nothing else about it is wrong");

    /* Hubbed: short from the first line, so the schema never drifts. This is
     * the scene that makes the stability check and the required-keys check
     * two checks. */
    ok(run_scene(P07_SCENE_HUBBED, 20u, "iks4a1", &l, &v),
       "a capture that was short from its first line");
    ok(v.keys_stable,
       "has a perfectly stable schema, which is why a stability check alone "
       "would pass it");
    ok(!v.required_present && !p07_verdict_passed(&v),
       "and is refused for the keys it never had, not for changing them");
    ok(strcmp(v.missing, "t") == 0, "the first missing key is named");
    ok(others_hold(&v, "required"), "and nothing else about it is wrong");

    /* Collapsed: wrong in two ways, and both of them are what collapsing
     * means. */
    ok(run_scene(P07_SCENE_COLLAPSED, 20u, "lowg", &l, &v),
       "a capture whose two streams were merged into one field");
    ok(!v.nothing_forbidden && strcmp(v.present, "acc") == 0,
       "carries the key P08 names");
    ok(!v.required_present && strcmp(v.missing, "lowg") == 0,
       "and has lost the one it was supposed to carry, which is the other "
       "half of the same mistake");
    ok(!p07_verdict_passed(&v), "and is refused");

    /* Noscale: a pressure with two possible meanings. */
    ok(run_scene(P07_SCENE_NOSCALE, 20u, "lowg", &l, &v),
       "a capture whose pressure has no range beside it");
    ok(!v.scale_with_pressure && l.pressure_without_scale == 20u,
       "is refused on every record, because the part has two scales and the "
       "number alone has two meanings");
    ok(others_hold(&v, "scale"), "and nothing else about it is wrong");

    printf("\n   lines that are not records\n");
    ok(write_text(TMP_A, "hello\n{\"lowg\":1.0}\nalso not json\n"),
       "a file with two lines that are not JSON objects");
    const p07_profile *p = p07_profile_of("lowg");
    ok(p07_read(TMP_A, p, &l) == P07_OK && l.lines == 3u && l.records == 1u &&
       l.refused == 2u,
       "counts them and drops them, rather than half reading either");

    ok(write_text(TMP_A, "not json at all\n") &&
       p07_read(TMP_A, p, &l) == P07_OK, "a file with no record in it reads");
    p07_judge(&l, p, &v);
    ok(!v.has_records && !p07_verdict_passed(&v),
       "and is refused for holding no record, rather than passing because "
       "nothing in it was wrong");

    ok(p07_read("test_p07_no_such_file", p, &l) == P07_E_OPEN,
       "a capture that is not there is refused by name");
    ok(p07_read(NULL, p, &l) == P07_E_ARG && p07_read(TMP_A, p, NULL) == P07_E_ARG,
       "and a caller's mistake is named as one");
}

/* ----------------------------------------------------------------- pair */

static void test_pair(void)
{
    printf("\n-- P08's third item, which is an item about P07\n");

    ok(scene_file(P07_SCENE_LOWG, 10u, TMP_A) &&
       scene_file(P07_SCENE_HIGHG, 10u, TMP_B) &&
       p07_compare(TMP_A, TMP_B) == P07_PAIR_OK,
       "the low-g file and the high-g file are two files with two schemas");

    ok(p07_compare(TMP_A, TMP_A) == P07_PAIR_SAME_PATH,
       "one file given twice is one log, not two");

    ok(scene_file(P07_SCENE_LOWG, 10u, TMP_B) &&
       p07_compare(TMP_A, TMP_B) == P07_PAIR_SAME_KEYS,
       "and two files carrying one schema are the same stream written down "
       "twice, which is the item failing in the way that is hardest to see "
       "by eye");

    ok(scene_file(P07_SCENE_IKS4A1, 10u, TMP_B) &&
       p07_compare(TMP_A, TMP_B) == P07_PAIR_OK,
       "P07's capture and P08's low-g capture are properly different files");

    ok(p07_compare(TMP_A, "test_p07_no_such_file") == P07_PAIR_UNREADABLE,
       "and a comparison against a file that is not there says so rather "
       "than passing");
    ok(p07_compare(NULL, TMP_A) == P07_PAIR_UNREADABLE,
       "and so does one against nothing");
}

/* --------------------------------------------------------------- status */

static void test_status(void)
{
    printf("\n-- every status and every pair result says something once\n");

    const p07_status ST[] = { P07_OK, P07_E_ARG, P07_E_OPEN, P07_E_EMPTY };
    const unsigned sn = (unsigned)(sizeof ST / sizeof ST[0]);
    unsigned distinct = 0;
    for (unsigned i = 0; i < sn; i++) {
        bool clash = false;
        for (unsigned j = 0; j < sn; j++) {
            if (i != j && strcmp(p07_strstatus(ST[i]),
                                 p07_strstatus(ST[j])) == 0)
                clash = true;
        }
        if (!clash && p07_strstatus(ST[i])[0] != '\0')
            distinct++;
    }
    ok(distinct == sn, "all four statuses have their own sentence");

    const p07_pair PR[] = { P07_PAIR_OK, P07_PAIR_SAME_PATH,
                            P07_PAIR_SAME_KEYS, P07_PAIR_UNREADABLE };
    const unsigned pn = (unsigned)(sizeof PR / sizeof PR[0]);
    distinct = 0;
    for (unsigned i = 0; i < pn; i++) {
        bool clash = false;
        for (unsigned j = 0; j < pn; j++) {
            if (i != j && strcmp(p07_strpair(PR[i]), p07_strpair(PR[j])) == 0)
                clash = true;
        }
        if (!clash && p07_strpair(PR[i])[0] != '\0')
            distinct++;
    }
    ok(distinct == pn, "and all four pair results do too");
}

int main(void)
{
    printf("P07 and P08 suite: no shield, no Nucleo, no capture\n");

    test_keys();
    test_profiles();
    test_logs();
    test_pair();
    test_status();
    remove(TMP_A);
    remove(TMP_B);

    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    if (g_failed > 0)
        return 1;
    printf("No shield was stacked. Every capture this suite read was one it\n"
           "wrote a moment earlier, and every key it looked for is quoted\n"
           "from a chapter rather than chosen here.\n");
    return 0;
}
