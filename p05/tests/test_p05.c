/* The suite. No hardware, no board, no voltmeter.
 *
 * Four groups, kept apart because they fail for different reasons:
 *
 *   file     whether a capture that looks finished and is not is caught,
 *            which a row count alone does not do
 *   book     whether an absent reading survives a write and a read as
 *            absent, rather than as a zero
 *   gate     whether the eight items are judged separately, and whether
 *            the three outcomes stay three
 *   bench    whether this lab's own conclusion holds: every item that can
 *            be settled here passes, and one cannot be settled here at all
 */

#include "../src/p05.h"

#include <math.h>
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

static const char *TMP_CSV  = "test_p05.csv";
static const char *TMP_BOOK = "test_p05.book";

static void good_book(p05_book *b, bool with_instrument)
{
    p05_book_init(b);
    snprintf(b->date, sizeof b->date, "Saturday 17 October 2026");
    b->have_board = true;
    b->board_address = 0;
    b->have_header_clear = true;
    b->header_clear = true;
    b->have_survived_reboot = true;
    b->survived_reboot = true;
    b->have_second_run = true;
    b->second_run_clean = true;
    if (with_instrument) {
        snprintf(b->instrument, sizeof b->instrument, "a multimeter that does "
                 "not exist on this bench");
        b->have_rail_3v3 = true;
        b->rail_3v3_v = 3.3012;
        b->have_rail_5v = true;
        b->rail_5v_v = 5.02;
    }
}

/* ---------------------------------------------------------------- groups */

static void test_file(void)
{
    printf("\n-- the file\n");

    p05_scan s;
    ok(p05_scene_write(P05_SCENE_GOOD, 10000, TMP_CSV) == P05_OK,
       "a whole one second capture is written");
    ok(p05_scan_read(TMP_CSV, &s) == P05_OK, "it reads");
    ok(s.rows == 10000, "ten thousand rows");
    ok(s.channels == 2, "two channels");
    ok(s.header_seen && s.starts_at_zero, "a header, and an index from zero");
    ok(s.holes == 0 && s.repeats == 0 && s.backwards == 0, "and no faults");
    ok(p05_scan_whole(&s), "so the file is whole");
    ok(fabs(s.mean[0] - P05_RAIL_3V3) < 0.01, "ch0 sits on the 3.3 V rail");
    ok(fabs(s.mean[1] - P05_RAIL_5V) < 0.01, "and ch1 on the 5 V rail");

    /* The three a row count alone would miss. */
    ok(p05_scene_write(P05_SCENE_HOLE, 10000, TMP_CSV) == P05_OK,
       "a capture with seven samples missing from the middle");
    ok(p05_scan_read(TMP_CSV, &s) == P05_OK, "it reads");
    ok(s.holes == 7, "the seven are reported as holes");
    ok(!p05_scan_whole(&s), "and the file is not whole");

    ok(p05_scene_write(P05_SCENE_REPEAT, 10000, TMP_CSV) == P05_OK,
       "a capture with one index written twice");
    ok(p05_scan_read(TMP_CSV, &s) == P05_OK, "it reads");
    ok(s.rows == 10001, "which has MORE rows than a whole one");
    ok(s.repeats == 1 && !p05_scan_whole(&s),
       "so a row count alone would call it complete, and the index does not");

    ok(p05_scene_write(P05_SCENE_SHORT, 10000, TMP_CSV) == P05_OK,
       "a capture that stopped early");
    ok(p05_scan_read(TMP_CSV, &s) == P05_OK, "it reads");
    ok(s.rows == 9975, "twenty five rows short");
    ok(p05_scan_whole(&s),
       "and it is whole: nothing is missing from the middle, it just ends, "
       "which is why the count is a separate item from the holes");

    ok(p05_scene_write(P05_SCENE_OVER_RANGE, 1000, TMP_CSV) == P05_OK,
       "a capture with one sample past the input limit");
    ok(p05_scan_read(TMP_CSV, &s) == P05_OK, "it reads");
    ok(s.out_of_range == 1,
       "the sample beyond 10.1 V is counted, because that is a wiring "
       "question arriving as a data question");

    /* Rows that are not rows. */
    FILE *fp = fopen(TMP_CSV, "wb");
    ok(fp != NULL, "a file with rubbish in it is written");
    if (fp != NULL) {
        fprintf(fp, "index,ch0,ch1\n0,3.3,5.0\n1,oops,5.0\n2,3.3,5.0\n");
        fclose(fp);
    }
    ok(p05_scan_read(TMP_CSV, &s) == P05_OK, "it reads");
    ok(s.rows == 2 && s.malformed == 1,
       "the row that does not parse is counted and dropped, not read as zero");
    ok(s.holes == 1,
       "and it leaves a hole, because the index it carried never arrived");
}

static void test_book(void)
{
    printf("\n-- the book\n");

    p05_book b;
    good_book(&b, false);
    ok(p05_book_write(&b, TMP_BOOK) == P05_OK, "a book with no rail readings");

    p05_book back;
    ok(p05_book_read(&back, TMP_BOOK) == P05_OK, "it reads back");
    ok(!back.have_rail_3v3,
       "and the absent reading is still absent, rather than a zero");
    ok(back.have_board && back.board_address == 0, "the board address survives");
    ok(back.have_header_clear && back.header_clear, "and the header fact");
    ok(strcmp(back.date, "Saturday 17 October 2026") == 0, "and the date");

    good_book(&b, true);
    ok(p05_book_write(&b, TMP_BOOK) == P05_OK, "a book with rail readings");
    ok(p05_book_read(&back, TMP_BOOK) == P05_OK, "it reads back");
    ok(back.have_rail_3v3 && fabs(back.rail_3v3_v - 3.3012) < 1e-6,
       "the reading survives to four decimal places");
    ok(strstr(back.instrument, "multimeter") != NULL,
       "and so does the name of whatever took it");

    /* A reading of zero volts is a reading. */
    p05_book_init(&b);
    b.have_rail_3v3 = true;
    b.rail_3v3_v = 0.0;
    ok(p05_book_write(&b, TMP_BOOK) == P05_OK, "a book recording zero volts");
    ok(p05_book_read(&back, TMP_BOOK) == P05_OK, "it reads back");
    ok(back.have_rail_3v3 && back.rail_3v3_v == 0.0,
       "which is present and zero, not absent");
}

static void test_gate(void)
{
    printf("\n-- the gate\n");

    p05_scan s;
    p05_book b;
    p05_verdict v;

    ok(p05_scene_write(P05_SCENE_GOOD, 10000, TMP_CSV) == P05_OK, "a whole capture");
    ok(p05_scan_read(TMP_CSV, &s) == P05_OK, "it reads");

    /* With an instrument reading that agrees: a full pass. */
    good_book(&b, true);
    ok(p05_judge(&s, &b, &v) == P05_OK, "it is judged against a complete book");
    ok(v.ch0_on_rail == P05_ITEM_PASS, "ch0 is on the rail");
    ok(v.sample_count == P05_ITEM_PASS, "the count is right");
    ok(v.index_monotonic == P05_ITEM_PASS, "the index is monotonic");
    ok(v.instrument_agrees == P05_ITEM_PASS, "the instrument agrees");
    ok(p05_verdict_passed(&v), "and every item passes");

    /* With an instrument reading that disagrees: a failure, not an absence. */
    good_book(&b, true);
    b.rail_3v3_v = 3.50;
    ok(p05_judge(&s, &b, &v) == P05_OK, "judged against a disagreeing reading");
    ok(v.instrument_agrees == P05_ITEM_FAIL,
       "which is a failure, because two instruments were compared and differed");
    ok(!p05_verdict_passed(&v) && !p05_verdict_passed_what_is_possible(&v),
       "and neither verdict calls that acceptable");

    /* With no instrument at all: not a failure, and not a pass. */
    good_book(&b, false);
    ok(p05_judge(&s, &b, &v) == P05_OK, "judged against this bench's book");
    ok(v.instrument_agrees == P05_ITEM_IMPOSSIBLE,
       "the item is marked as one this bench cannot settle");
    ok(!p05_verdict_passed(&v), "so the acceptance test is not met");
    ok(p05_verdict_passed_what_is_possible(&v),
       "and everything that can be settled here is");

    /* An unrecorded operator fact is a third thing again: fixable by
     * walking back to the bench, which the impossible item is not. */
    good_book(&b, true);
    b.have_second_run = false;
    ok(p05_judge(&s, &b, &v) == P05_OK, "judged with one fact unwritten");
    ok(v.second_run_clean == P05_ITEM_UNRECORDED,
       "it reads as unrecorded rather than as failed");
    ok(!p05_verdict_passed_what_is_possible(&v),
       "and an unrecorded item is not excused the way an impossible one is");

    /* A board at the wrong address is a failure, not an absence. */
    good_book(&b, true);
    b.board_address = 1;
    ok(p05_judge(&s, &b, &v) == P05_OK, "judged with the board at address one");
    ok(v.board_named == P05_ITEM_FAIL, "which is a failure");
}

static void test_bench(void)
{
    printf("\n-- this bench, and the one thing it cannot buy with time\n");

    p05_scan s;
    p05_book b;
    p05_verdict v;

    /* The claim STATUS.md makes about this lab, as an assertion. Give the
     * best run this room can produce and the best book it can write, and
     * the answer is still one item short. */
    ok(p05_scene_write(P05_SCENE_GOOD, 10000, TMP_CSV) == P05_OK,
       "the best capture this bench can take");
    ok(p05_scan_read(TMP_CSV, &s) == P05_OK, "it reads");
    good_book(&b, false);
    snprintf(b.os_release, sizeof b.os_release, "Debian GNU/Linux 12 (bookworm)");
    snprintf(b.kernel, sizeof b.kernel, "Linux 6.6.0 aarch64");
    snprintf(b.model, sizeof b.model, "Raspberry Pi 4 Model B Rev 1.4");
    snprintf(b.root_fs, sizeof b.root_fs, "/dev/mmcblk0p2");
    ok(p05_judge(&s, &b, &v) == P05_OK, "the best book this bench can write");

    ok(v.ch0_on_rail == P05_ITEM_PASS && v.ch1_near_5v == P05_ITEM_PASS &&
       v.sample_count == P05_ITEM_PASS && v.index_monotonic == P05_ITEM_PASS &&
       v.board_named == P05_ITEM_PASS && v.header_clear == P05_ITEM_PASS &&
       v.second_run_clean == P05_ITEM_PASS,
       "seven of the eight items pass");
    ok(v.instrument_agrees == P05_ITEM_IMPOSSIBLE,
       "and the eighth cannot be settled in this room at all");
    ok(!p05_verdict_passed(&v),
       "so this lab cannot reach a pass here, which is what STATUS.md says");
    ok(p05_verdict_passed_what_is_possible(&v),
       "and it is not failing either, which is the distinction the report "
       "exists to keep");

    /* And the same run with a multimeter in the room. */
    b.have_rail_3v3 = true;
    b.rail_3v3_v = 3.2988;
    snprintf(b.instrument, sizeof b.instrument, "a cheap multimeter");
    ok(p05_judge(&s, &b, &v) == P05_OK, "the same run, with one instrument added");
    ok(p05_verdict_passed(&v),
       "passes on every count, which is the whole of what that purchase buys");
}

static void cleanup(void)
{
    remove(TMP_CSV);
    remove(TMP_BOOK);
}

int main(void)
{
    printf("P05 suite: no hardware, no board, no voltmeter\n");

    test_file();
    test_book();
    test_gate();
    test_bench();
    cleanup();

    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    if (g_failed > 0)
        return 1;
    printf("No voltage here was measured. Every row came from a file this\n"
           "suite wrote a moment earlier.\n");
    return 0;
}
