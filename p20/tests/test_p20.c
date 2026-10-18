/* The suite. No SSH, no hosts, no sweep, and no file any bench wrote.
 *
 * Five groups:
 *
 *   fleet      whether the four hosts are the four hosts, and whether the
 *              NanoPi's two differences from a Pi are recorded
 *   parse      whether the lines that have their own shape are taken for
 *              what they are, rather than for host names
 *   absences   whether a missing line is counted as honest or as a gap
 *              according to which host it is missing under
 *   judge      whether each of the five bad sweeps is refused, and refused
 *              for its own reason and no other
 *   status     whether every status says something, and says it once
 *
 * The last part of the judge group is the one that earns its place. Each
 * scene is wrong in exactly one way, so for each of them the suite asserts
 * that the matching check failed **and that every other check still
 * passed**. A set of checks that all fail together is one check with five
 * names.
 */

#include "../src/p20.h"

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

static const char *TMP = "test_p20_fleet.txt";

static bool write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL)
        return false;
    fputs(text, f);
    fclose(f);
    return true;
}

static bool judge_scene(p20_scene_kind k, p20_sweep *s, p20_verdict *v)
{
    FILE *f = fopen(TMP, "wb");
    if (f == NULL)
        return false;
    p20_status st = p20_scene_write(k, f);
    fclose(f);
    if (st != P20_OK)
        return false;
    if (p20_read(TMP, s) != P20_OK)
        return false;
    p20_judge(s, v);
    return true;
}

/* ---------------------------------------------------------------- fleet */

static void test_fleet(void)
{
    printf("\n-- the fleet is four hosts, and one of them is not a Pi\n");

    const p20_host *f = p20_fleet();
    ok(strcmp(f[0].name, "pi4") == 0 && strcmp(f[1].name, "pi3bp") == 0 &&
       strcmp(f[2].name, "pi3") == 0 && strcmp(f[3].name, "air") == 0,
       "the four are pi4, pi3bp, pi3 and air, in the order the chapter's "
       "loop names them");

    ok(p20_host_of("pi4") != NULL && p20_host_of("air") != NULL,
       "each of them is found by name");
    ok(p20_host_of("pi5") == NULL,
       "and a fifth name is not, because the fleet is four hosts");
    ok(p20_host_of(NULL) == NULL, "nor is no name at all");

    ok(f[0].answers_temp && f[1].answers_temp && f[2].answers_temp,
       "the three Raspberry Pis answer vcgencmd");
    ok(!f[3].answers_temp,
       "and the NanoPi does not, because vcgencmd is a Raspberry Pi tool");
    ok(f[0].header_bus == 1u && f[3].header_bus == 0u,
       "the header bus is 1 on a Pi and 0 on the NanoPi, which P09 "
       "established on the bench");
}

/* ---------------------------------------------------------------- parse */

static void test_parse(void)
{
    printf("\n-- what a line is, before it is anything else\n");

    p20_sweep s;

    /* The trap this ordering exists for. vcgencmd prints a bare token with
     * no space in it, and a parser that looked for host names first would
     * read a temperature as a fifth host. */
    ok(write_text(TMP, "temp=46.2'C\n") && p20_read(TMP, &s) == P20_OK,
       "a temperature on its own is read");
    ok(s.strangers == 0u,
       "and is not taken for a host name, which is why the shaped lines are "
       "matched before the bare ones");

    ok(write_text(TMP, "pi4\nhat: MCC 118\n") && p20_read(TMP, &s) == P20_OK,
       "a host and a HAT");
    ok(s.b[0].seen && s.b[0].hat_reported && s.b[0].hat_named,
       "the host opened a block and the HAT was reported and named");
    ok(strcmp(s.b[0].hat, "MCC 118") == 0,
       "and the name carries its space, so it is the product string and not "
       "the first word of it");

    ok(write_text(TMP, "pi4\nhat: none\n") && p20_read(TMP, &s) == P20_OK,
       "a host whose header is empty");
    ok(s.b[0].hat_reported && !s.b[0].hat_named,
       "reports, and reports nothing seated, which are two different facts "
       "and both are recorded");

    ok(write_text(TMP, "pi4\nhat:\n") && p20_read(TMP, &s) == P20_OK,
       "a host whose HAT line is a label with nothing after it");
    ok(!s.b[0].hat_reported,
       "has not reported, because a blank is the pitfall and not a value");

    ok(write_text(TMP, "pi4\nuptime and load average: 0.08\n") &&
       p20_read(TMP, &s) == P20_OK && s.strangers == 0u,
       "ordinary block text is neither a field nor a host name");

    ok(p20_read("test_p20_no_such_file", &s) == P20_E_OPEN,
       "a fleet file that is not there is refused by name");
    ok(p20_read(NULL, &s) == P20_E_ARG && p20_read(TMP, NULL) == P20_E_ARG,
       "and a caller's mistake is named as one");
}

/* ------------------------------------------------------------- absences */

static void test_absences(void)
{
    printf("\n-- the same missing line, counted two ways\n");

    p20_sweep s;
    p20_verdict v;

    ok(judge_scene(P20_SCENE_GOOD, &s, &v), "a good sweep is read");
    ok(!s.b[3].temp_seen,
       "the NanoPi's block carries no temperature, as the chapter's sweep "
       "produces");
    ok(v.honest_absences == 1u,
       "and that is counted as one honest absence, because vcgencmd was "
       "never going to answer there");
    ok(v.gaps == 0u,
       "and as no gap at all, so a shorter block for that host is not held "
       "against the sweep");

    /* The file where both happen at once, which is the only way to show the
     * two counters are not the same counter. */
    ok(judge_scene(P20_SCENE_PI_NO_TEMP, &s, &v),
       "a sweep where a Pi did not answer vcgencmd either");
    ok(!s.b[1].temp_seen && !s.b[3].temp_seen,
       "two hosts are missing the same line");
    ok(v.gaps == 1u,
       "one of them is a gap, because a Raspberry Pi has vcgencmd and that "
       "command failed");
    ok(v.honest_absences == 1u,
       "and the other is still honest, in the same file, from the same "
       "missing line");
    ok(v.first_gap_host == 1u, "and the gap is named as the Pi 3B+");
}

/* ---------------------------------------------------------------- judge */

/* Every check except the one named. A scene is wrong in exactly one way, so
 * this is what makes five checks five checks rather than one with five
 * names. */
static bool others_hold(const p20_verdict *v, const char *except)
{
    bool four = v->four_hosts || strcmp(except, "four") == 0;
    bool hats = v->every_hat_reported || strcmp(except, "hat") == 0;
    bool excl = v->exclusive || strcmp(except, "exclusive") == 0;
    bool strg = v->no_stranger || strcmp(except, "stranger") == 0;
    bool date = v->dated || strcmp(except, "dated") == 0;
    bool gaps = (v->gaps == 0u) || strcmp(except, "gaps") == 0;
    return four && hats && excl && strg && date && gaps;
}

static void test_judge(void)
{
    printf("\n-- five sweeps, each wrong in exactly one way\n");

    p20_sweep s;
    p20_verdict v;

    ok(judge_scene(P20_SCENE_GOOD, &s, &v) && p20_verdict_passed(&v),
       "the good sweep is an acceptance artifact");
    ok(v.present == 4u, "with all four hosts present");

    ok(judge_scene(P20_SCENE_BLANK_HAT, &s, &v), "the blank-HAT sweep reads");
    ok(!v.every_hat_reported && !p20_verdict_passed(&v),
       "and is refused, because a missing line and an empty header must not "
       "look the same in the artifact");
    ok(v.first_silent_hat == 2u, "and the silent host is named as the Pi 3");
    ok(others_hold(&v, "hat"), "and nothing else about it is wrong");

    ok(judge_scene(P20_SCENE_FIFTH, &s, &v), "the fifth-host sweep reads");
    ok(!v.no_stranger && !p20_verdict_passed(&v),
       "and is refused, because a fifth name is a host this book has not "
       "brought up");
    ok(strcmp(s.stranger, "pi5") == 0, "and the stranger is named");
    ok(others_hold(&v, "stranger"), "and nothing else about it is wrong");

    ok(judge_scene(P20_SCENE_TWO_HATS, &s, &v), "the two-HAT sweep reads");
    ok(!v.exclusive && !p20_verdict_passed(&v),
       "and is refused, because one board owns the header and that is the "
       "rule every lab in this volume is written against");
    ok(v.doubled == 0u && s.b[0].hats_named == 2u,
       "and the host that named two is the Pi 4");
    ok(others_hold(&v, "exclusive"), "and nothing else about it is wrong");

    ok(judge_scene(P20_SCENE_UNDATED, &s, &v), "the undated sweep reads");
    ok(!v.dated && !p20_verdict_passed(&v),
       "and is refused, because a sweep taken during a teardown describes a "
       "bench that no longer exists and nothing says which this was");
    ok(others_hold(&v, "dated"), "and nothing else about it is wrong");

    ok(judge_scene(P20_SCENE_PI_NO_TEMP, &s, &v), "the silent-Pi sweep reads");
    ok(v.gaps == 1u && !p20_verdict_passed(&v),
       "and is refused, because a command that failed is not a command that "
       "is absent");
    ok(others_hold(&v, "gaps"), "and nothing else about it is wrong");

    /* A fleet file with nothing in it at all. */
    ok(write_text(TMP, "") && p20_read(TMP, &s) == P20_E_EMPTY,
       "an empty file is refused as empty rather than judged as a sweep "
       "where nobody answered");
}

/* --------------------------------------------------------------- status */

static void test_status(void)
{
    printf("\n-- every status says something, and says it only once\n");

    const p20_status ALL[] = { P20_OK, P20_E_ARG, P20_E_OPEN, P20_E_EMPTY };
    const unsigned n = (unsigned)(sizeof ALL / sizeof ALL[0]);
    unsigned named = 0;
    unsigned distinct = 0;
    for (unsigned i = 0; i < n; i++) {
        const char *a = p20_strstatus(ALL[i]);
        if (a != NULL && a[0] != '\0')
            named++;
        bool clash = false;
        for (unsigned j = 0; j < n; j++) {
            if (i != j && strcmp(a, p20_strstatus(ALL[j])) == 0)
                clash = true;
        }
        if (!clash)
            distinct++;
    }
    ok(named == n, "all four statuses have a sentence");
    ok(distinct == n, "and no two share one");
}

int main(void)
{
    printf("P20 suite: no SSH, no hosts, no sweep\n");

    test_fleet();
    test_parse();
    test_absences();
    test_judge();
    test_status();
    remove(TMP);

    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    if (g_failed > 0)
        return 1;
    printf("No host was reached. Every fleet file this suite judged was one\n"
           "it wrote a moment earlier, and each bad one is wrong in exactly\n"
           "one way, which is asserted rather than assumed.\n");
    return 0;
}
