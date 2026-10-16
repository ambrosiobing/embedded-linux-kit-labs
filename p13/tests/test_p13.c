/* The suite. No hardware, no broker, no radio, and no clock.
 *
 * Five groups, kept apart because they fail for different reasons:
 *
 *   tags      whether the source tag is read from the record rather than
 *             assumed, and whether a line without one is counted
 *   machine   whether the two states move only when they should, which
 *             includes not moving when the backup speaks
 *   window    whether the window is the window, at its own boundary
 *   scenes    whether a generated run replays to the same transitions
 *   gate      whether a log that does not show the antenna test is refused,
 *             including a log of a perfectly healthy run
 *
 * Nothing here sleeps or reads a clock. The machine takes its time as an
 * argument, so a ten minute run is tested in microseconds and the window
 * boundary is tested exactly rather than approximately.
 */

#include "../src/p13.h"

#include <stdio.h>
#include <stdlib.h>
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

static const char *TMP_LOG = "test_p13.p13";

static const char *CAT4 = "lab/p13/health {\"src\":\"cat4\",\"ok\":1}";
static const char *NB   = "lab/p13/health {\"src\":\"nb\",\"ok\":1}";

/* ---------------------------------------------------------------- groups */

static void test_tags(void)
{
    printf("\n-- tags\n");

    char tag[P13_TAG_MAX];
    ok(p13_tag_of(CAT4, tag, sizeof tag) && strcmp(tag, "cat4") == 0,
       "the primary tag is read out of the record");
    ok(p13_tag_of(NB, tag, sizeof tag) && strcmp(tag, "nb") == 0,
       "and so is the backup tag");
    ok(p13_tag_of("x {\"src\": \"cat4\"}", tag, sizeof tag) && strcmp(tag, "cat4") == 0,
       "a space after the colon changes nothing");
    ok(!p13_tag_of("lab/p13/health {\"ok\":1}", tag, sizeof tag),
       "a record with no source field has no tag, rather than a guessed one");
    ok(!p13_tag_of("lab/p13/health {\"src\":\"\"}", tag, sizeof tag),
       "an empty source is not a tag");
    ok(!p13_tag_of("", tag, sizeof tag), "and an empty line is not a record");
}

static void test_machine(void)
{
    printf("\n-- the machine\n");

    p13_supervisor s;
    p13_init(&s, 45.0);
    ok(s.state == P13_FAILOVER,
       "a supervisor that has heard nothing starts in FAILOVER");
    ok(p13_on_tick(&s, 1000.0) == P13_EVENT_NONE,
       "and no amount of silence moves it, because silence is not news");

    ok(p13_on_record(&s, 0.0, "cat4", CAT4) == P13_EVENT_TO_PRIMARY,
       "the first primary record takes it to PRIMARY");
    ok(s.state == P13_PRIMARY, "where it stays");
    ok(p13_on_record(&s, 15.0, "cat4", CAT4) == P13_EVENT_NONE,
       "a second primary record is not a transition");

    ok(p13_on_record(&s, 20.0, "nb", NB) == P13_EVENT_NONE,
       "a backup record never changes the state");
    ok(s.state == P13_PRIMARY && s.have_backup,
       "it is kept as the live reading and nothing else");

    ok(p13_on_tick(&s, 40.0) == P13_EVENT_NONE,
       "a tick inside the window does nothing");
    ok(p13_on_tick(&s, 70.0) == P13_EVENT_TO_FAILOVER,
       "a tick past it falls to FAILOVER");
    ok(s.state == P13_FAILOVER, "and it stays fallen");
    ok(p13_on_tick(&s, 80.0) == P13_EVENT_NONE,
       "falling twice is not a second transition");

    ok(p13_on_record(&s, 90.0, "nb", NB) == P13_EVENT_NONE,
       "the backup speaking does not bring the state back");
    ok(s.state == P13_FAILOVER,
       "because a route is not declared good because a different route spoke");

    ok(p13_on_record(&s, 95.0, "cat4", CAT4) == P13_EVENT_TO_PRIMARY,
       "the next primary record does bring it back");
    ok(s.to_failover == 1 && s.to_primary == 2,
       "one fall and two rises, counting the one at the start of the run");

    ok(p13_on_record(&s, 96.0, "wifi", "x") == P13_EVENT_NONE,
       "a tag this lab does not know is not acted on");
    ok(s.unreadable == 1, "and is counted");
}

static void test_window(void)
{
    printf("\n-- the window, at its boundary\n");

    p13_supervisor s;
    p13_init(&s, 45.0);
    p13_on_record(&s, 0.0, "cat4", CAT4);
    ok(p13_on_tick(&s, 45.0) == P13_EVENT_NONE,
       "at exactly forty five seconds the primary is late but not missing");
    ok(p13_on_tick(&s, 45.001) == P13_EVENT_TO_FAILOVER,
       "and a thousandth of a second later it is missing");

    /* The window is the only tunable, so it has to actually tune. */
    p13_init(&s, 10.0);
    p13_on_record(&s, 0.0, "cat4", CAT4);
    ok(p13_on_tick(&s, 10.5) == P13_EVENT_TO_FAILOVER,
       "a ten second window falls at ten and a half");

    p13_init(&s, 120.0);
    p13_on_record(&s, 0.0, "cat4", CAT4);
    ok(p13_on_tick(&s, 100.0) == P13_EVENT_NONE,
       "and a two minute window is still up at a hundred seconds");
}

static void test_scenes(void)
{
    printf("\n-- the scenes, and the replay\n");

    p13_synth cfg;
    p13_synth_init(&cfg);
    p13_supervisor s;

    ok(p13_scene_write(P13_SCENE_OUTAGE, &cfg, 45.0, TMP_LOG) == P13_OK,
       "an outage scene is written");
    ok(p13_replay(TMP_LOG, 45.0, &s) == P13_OK, "and replayed");
    ok(s.primary_records == 34,
       "thirty four primary records, with two minutes of them missing");
    ok(s.backup_records == 6, "six backup records, which never stopped");
    ok(s.to_failover == 1, "one fall");
    ok(s.to_primary == 2, "and two rises, the first being the start of the run");
    ok(s.state == P13_PRIMARY, "the run ends in PRIMARY");

    ok(p13_scene_write(P13_SCENE_STEADY, &cfg, 45.0, TMP_LOG) == P13_OK,
       "a steady scene is written");
    ok(p13_replay(TMP_LOG, 45.0, &s) == P13_OK, "and replayed");
    ok(s.to_failover == 0, "a run with no outage in it never falls");

    ok(p13_scene_write(P13_SCENE_SHORT_GAP, &cfg, 45.0, TMP_LOG) == P13_OK,
       "a scene missing one primary sample");
    ok(p13_replay(TMP_LOG, 45.0, &s) == P13_OK, "and replayed");
    ok(s.to_failover == 0,
       "one missed sample is inside the window, which is why the window is "
       "three samples and not one");

    ok(p13_scene_write(P13_SCENE_NO_RETURN, &cfg, 45.0, TMP_LOG) == P13_OK,
       "a scene where the primary never comes back");
    ok(p13_replay(TMP_LOG, 45.0, &s) == P13_OK, "and replayed");
    ok(s.to_failover == 1 && s.state == P13_FAILOVER,
       "it falls once and stays fallen");

    ok(p13_scene_write(P13_SCENE_BACKUP_ONLY, &cfg, 45.0, TMP_LOG) == P13_OK,
       "a scene with no primary at all");
    ok(p13_replay(TMP_LOG, 45.0, &s) == P13_OK, "and replayed");
    ok(s.to_failover == 0 && s.to_primary == 0 && s.state == P13_FAILOVER,
       "nothing ever transitions, because nothing was ever heard from the primary");
    ok(s.backup_records > 0, "though the backup was heard throughout");
}

static void test_gate(void)
{
    printf("\n-- the gate\n");

    p13_synth cfg;
    p13_synth_init(&cfg);
    p13_verdict v;

    ok(p13_scene_write(P13_SCENE_OUTAGE, &cfg, 45.0, TMP_LOG) == P13_OK,
       "the outage scene again");
    ok(p13_verify(TMP_LOG, 45.0, &v) == P13_OK, "it is judged");
    ok(v.both_sources, "both sources are in the log");
    ok(v.flipped && v.returned, "it fell and came back");
    ok(v.flip_in_window,
       "the fall was no later than the window plus the tick that noticed it");
    ok(v.worst_flip_lateness <= 1.0,
       "which is within one second of the window exactly");
    ok(v.return_on_record,
       "and the return landed on a primary record rather than on a timer");
    ok(v.backup_rhythm_kept,
       "the backup kept its rhythm across the fall, because the two are not "
       "connected");
    ok(p13_verdict_passed(&v), "so the acceptance test is met");

    /* A healthy run is not a demonstration of a failover test, and saying
     * so is the whole point of having a gate rather than a summary. */
    ok(p13_scene_write(P13_SCENE_STEADY, &cfg, 45.0, TMP_LOG) == P13_OK,
       "a steady scene");
    ok(p13_verify(TMP_LOG, 45.0, &v) == P13_OK, "it is judged");
    ok(v.both_sources, "both sources are there");
    ok(!v.flipped && !p13_verdict_passed(&v),
       "and it is refused, because nobody pulled an antenna");

    ok(p13_scene_write(P13_SCENE_NO_RETURN, &cfg, 45.0, TMP_LOG) == P13_OK,
       "a scene that falls and never returns");
    ok(p13_verify(TMP_LOG, 45.0, &v) == P13_OK, "it is judged");
    ok(v.flipped && !v.returned,
       "half the test is shown and the other half is not");
    ok(!p13_verdict_passed(&v), "which is not a pass");

    ok(p13_scene_write(P13_SCENE_BACKUP_ONLY, &cfg, 45.0, TMP_LOG) == P13_OK,
       "a scene with no primary");
    ok(p13_verify(TMP_LOG, 45.0, &v) == P13_OK, "it is judged");
    ok(!v.both_sources && !p13_verdict_passed(&v),
       "one source is not two, and a log with one is refused");

    /* A log judged against the wrong window. The log is a record of a run
     * at 45 seconds, and reading it as though the window were 10 makes the
     * fall look eleven times later than it should be. */
    ok(p13_scene_write(P13_SCENE_OUTAGE, &cfg, 45.0, TMP_LOG) == P13_OK,
       "the outage scene once more");
    ok(p13_verify(TMP_LOG, 10.0, &v) == P13_OK, "judged against a ten second window");
    ok(!v.flip_in_window && !p13_verdict_passed(&v),
       "and refused, because the window a log is judged against has to be the "
       "window it was run at");
}

static void cleanup(void)
{
    remove(TMP_LOG);
}

int main(void)
{
    printf("P13 suite: no hardware, no broker, no radio, no clock\n");

    test_tags();
    test_machine();
    test_window();
    test_scenes();
    test_gate();
    cleanup();

    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    if (g_failed > 0)
        return 1;
    printf("No antenna was unscrewed. Every record came from a file this\n"
           "suite wrote a moment earlier.\n");
    return 0;
}
