/* The suite. No instrument, no DUT, no USB cable, and no trace that was
 * ever taken.
 *
 * Six groups, kept apart because they fail for different reasons:
 *
 *   format     whether a nanoamp count prints as a figure a lab book would
 *              carry, including at the one awkward boundary
 *   histogram  whether the quantiles are the quantiles, against values
 *              worked out on paper rather than read off this program
 *   parse      whether a line that is not a sample is counted and dropped
 *              rather than guessed at
 *   book       whether the lab book survives a trip to a file and back
 *   judge      whether the four comparisons refuse what they exist to refuse
 *   order      whether the order is measured rather than taken from the
 *              names the firmware was given
 *
 * Every expected value below was computed from the input, not written to
 * agree with the sentence describing it. The ramp exists for exactly that:
 * its samples are 1, 2, 3 microamps and so on, so the rank of a quantile and
 * the value at that rank are arithmetic.
 */

#include "../src/p10.h"

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

static const char *TMP_CAP = "test_p10_capture.csv";
static const char *TMP_BOOK = "test_p10.p10book";

static bool write_lines(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL)
        return false;
    fputs(text, f);
    fclose(f);
    return true;
}

static bool write_ramp(const char *path, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL)
        return false;
    p10_status s = p10_scene_write(P10_SCENE_RAMP, n, f);
    fclose(f);
    return s == P10_OK;
}

/* A reduction with the two numbers the judge actually reads. */
static p10_reduction red(uint32_t median, uint32_t p95)
{
    p10_reduction r;
    memset(&r, 0, sizeof r);
    r.n = 1000u;
    r.lo = median;
    r.hi = p95;
    r.median = median;
    r.p95 = p95;
    r.bucket = 1u;
    return r;
}

/* --------------------------------------------------------------- format */

static void test_format(void)
{
    printf("\n-- a nanoamp count as a figure a lab book would carry\n");

    char b[24];
    ok(p10_format_current(b, sizeof b, 45000u) && strcmp(b, "45.0 uA") == 0,
       "45000 nA is 45.0 uA, which is a dev-board deep sleep");
    ok(p10_format_current(b, sizeof b, 500102u) && strcmp(b, "500.1 uA") == 0,
       "500102 nA is 500.1 uA");
    ok(p10_format_current(b, sizeof b, 0u) && strcmp(b, "0.0 uA") == 0,
       "and nothing is 0.0 uA rather than a blank");

    ok(p10_format_current(b, sizeof b, 1000000u) && strcmp(b, "1.0 mA") == 0,
       "1000000 nA crosses into milliamps and is 1.0 mA");
    ok(p10_format_current(b, sizeof b, 19930000u) && strcmp(b, "19.9 mA") == 0,
       "19930000 nA is 19.9 mA");
    ok(p10_format_current(b, sizeof b, 110000000u) &&
       strcmp(b, "110.0 mA") == 0,
       "110000000 nA is 110.0 mA, which is a BLE advertising burst");

    /* The one awkward value, asserted so that it is a known property of
     * this formatter rather than something a reader meets by surprise. */
    ok(p10_format_current(b, sizeof b, 999999u) && strcmp(b, "1000.0 uA") == 0,
       "999999 nA prints as 1000.0 uA, because the switch to milliamps "
       "happens at the value and not at what it rounds to");

    ok(!p10_format_current(b, 4u, 19930000u),
       "a buffer too small is refused rather than filled with half a number");
}

/* ------------------------------------------------------------ histogram */

static void test_histogram(void)
{
    printf("\n-- the quantiles, against arithmetic rather than observation\n");

    static p10_hist h;

    p10_hist_init(&h, 0u, 0u);
    ok(h.n == 0u && p10_hist_quantile(&h, P10_MEDIAN) == 0u,
       "an empty histogram has no median, and says so with a zero count "
       "rather than with a zero value a caller might believe");

    p10_hist_init(&h, 7000u, 7000u);
    p10_hist_add(&h, 7000u);
    p10_hist_add(&h, 7000u);
    ok(p10_hist_bucket(&h) == 1u,
       "a trace whose samples are all one value has a bucket of one, not of "
       "zero");
    ok(p10_hist_quantile(&h, P10_MEDIAN) == 7000u &&
       p10_hist_quantile(&h, P10_P95) == 7000u,
       "and both of its quantiles are that value exactly");

    /* The ramp, 1000 samples of 1, 2, ... 1000 microamps.
     *
     *   lo   = 1000 nA, hi = 1000000 nA
     *   span = 1000000 - 1000 + 1 = 999001
     *   bucket = ceil(999001 / 4096) = 244
     *   median rank = ceil(1000 * 500 / 1000) = 500, so the 500th sample,
     *                 which is 500 uA, which is 500000 nA
     *   p95 rank    = ceil(1000 * 950 / 1000) = 950, the 950th sample,
     *                 950 uA, 950000 nA
     *
     * The histogram can only answer to a bucket, so each is asserted to be
     * within one bucket of the value computed above, and the bucket itself
     * is asserted exactly. */
    ok(write_ramp(TMP_CAP, 1000u), "a thousand-sample ramp is written");

    p10_reduction r;
    ok(p10_reduce_file(TMP_CAP, 0u, &r) == P10_OK, "and reduced");
    ok(r.n == 1000u && r.refused == 0u,
       "all thousand samples were read and none refused, so the comment "
       "lines are not being counted as failures");
    ok(r.lo == 1000u && r.hi == 1000000u,
       "the range is 1 uA to 1000 uA, in nanoamps");
    ok(r.bucket == 244u,
       "the bucket is 244 nA, which is ceil(999001 / 4096)");

    uint32_t want_med = 500000u;
    uint32_t want_p95 = 950000u;
    ok(r.median >= want_med - r.bucket && r.median <= want_med + r.bucket,
       "the median is within one bucket of the 500th sample");
    ok(r.p95 >= want_p95 - r.bucket && r.p95 <= want_p95 + r.bucket,
       "and the p95 is within one bucket of the 950th sample");
    ok(r.median < r.p95,
       "and the p95 is above the median, which a rising ramp requires");

    /* A hundred-sample ramp, so the arithmetic is checked at a second size
     * rather than at the one it was derived on. span = 99001, bucket =
     * ceil(99001 / 4096) = 25; median rank 50, p95 rank 95. */
    ok(write_ramp(TMP_CAP, 100u), "a hundred-sample ramp is written");
    ok(p10_reduce_file(TMP_CAP, 0u, &r) == P10_OK, "and reduced");
    ok(r.bucket == 25u, "its bucket is 25 nA, which is ceil(99001 / 4096)");
    ok(r.median >= 50000u - r.bucket && r.median <= 50000u + r.bucket,
       "its median is within one bucket of the 50th sample");
    ok(r.p95 >= 95000u - r.bucket && r.p95 <= 95000u + r.bucket,
       "and its p95 within one bucket of the 95th");
}

/* --------------------------------------------------------------- parse */

static void test_parse(void)
{
    printf("\n-- what is a sample, and what is counted and dropped\n");

    p10_reduction r;

    ok(write_lines(TMP_CAP, "# a comment\n500\n500\n500\n"),
       "three identical samples behind a comment");
    ok(p10_reduce_file(TMP_CAP, 0u, &r) == P10_OK && r.n == 3u &&
       r.refused == 0u,
       "a comment line is skipped and is not a refusal");
    ok(r.median == 500000u,
       "and three samples of 500 uA have a median of exactly 500000 nA");

    ok(write_lines(TMP_CAP, "1234.56\n"), "a sample with a fractional part");
    ok(p10_reduce_file(TMP_CAP, 0u, &r) == P10_OK && r.median == 1234560u,
       "1234.56 uA is 1234560 nA, so three decimal places survive");

    ok(write_lines(TMP_CAP, "Timestamp,Current(uA)\n0.0,2000\n0.1,2000\n"),
       "a vendor export with its own header line");
    ok(p10_reduce_file(TMP_CAP, 1u, &r) == P10_OK && r.n == 2u &&
       r.refused == 1u,
       "the header is refused and counted, and the second column is read");
    ok(r.median == 2000000u, "the two samples are 2 mA");

    ok(write_lines(TMP_CAP, "100\n-5\n200\n"), "a capture with a negative");
    ok(p10_reduce_file(TMP_CAP, 0u, &r) == P10_OK && r.n == 2u &&
       r.refused == 1u,
       "the negative is refused and counted, not clamped to zero, because "
       "clamping moves a median by an amount nobody could see afterwards");

    ok(write_lines(TMP_CAP, "100abc\n"), "a number with rubbish after it");
    ok(p10_reduce_file(TMP_CAP, 0u, &r) == P10_E_EMPTY,
       "is not a sample, and a capture of nothing else is empty rather than "
       "a reduction of one value");

    ok(write_lines(TMP_CAP, "# only a comment\n"), "a capture with no sample");
    ok(p10_reduce_file(TMP_CAP, 0u, &r) == P10_E_EMPTY,
       "is refused as empty");

    ok(p10_reduce_file("-", 0u, &r) == P10_E_ARG,
       "standard input is refused, because two passes need a file and this "
       "says so rather than reading half the capture twice");
    ok(p10_reduce_file("test_p10_no_such_file", 0u, &r) == P10_E_OPEN,
       "a capture that is not there is refused by name");
}

/* ---------------------------------------------------------------- book */

static void test_book(void)
{
    printf("\n-- the lab book, to a file and back\n");

    p10_book b;
    p10_book_init(&b);
    p10_reduction s = red(45000u, 52000u);
    p10_reduction i = red(22000000u, 24000000u);
    ok(p10_book_add(&b, "sleep", &s, "the USB-UART silicon leaks") == P10_OK,
       "a state with its sentence goes in");
    ok(p10_book_add(&b, "idle", &i, NULL) == P10_OK, "and one without");
    ok(b.n == 2u, "the book holds two");

    FILE *f = fopen(TMP_BOOK, "wb");
    ok(f != NULL && p10_book_write(&b, f) == P10_OK, "it writes");
    if (f != NULL)
        fclose(f);

    p10_book back;
    ok(p10_book_read(&back, TMP_BOOK) == P10_OK, "and reads back");
    ok(back.n == 2u, "with both entries");
    const p10_entry *e = p10_book_find(&back, "sleep");
    ok(e != NULL && e->r.median == 45000u && e->r.p95 == 52000u,
       "and the two numbers survived the trip exactly");
    ok(e != NULL && strcmp(e->reason, "the USB-UART silicon leaks") == 0,
       "and so did the sentence, spaces and all");
    e = p10_book_find(&back, "idle");
    ok(e != NULL && e->reason[0] == '\0',
       "a state with no sentence comes back with none, rather than with the "
       "previous one");
    ok(p10_book_find(&back, "ble") == NULL,
       "and a state nobody captured is absent rather than empty");

    ok(write_lines(TMP_BOOK, "state sleep n=1\n") &&
       p10_book_read(&back, TMP_BOOK) == P10_E_FORMAT,
       "a half-written state line is refused rather than filled in with "
       "zeroes");
    ok(write_lines(TMP_BOOK, "why nobody a reason for a state that is not "
                             "here\n") &&
       p10_book_read(&back, TMP_BOOK) == P10_E_FORMAT,
       "and so is a sentence attached to a state nobody captured");
}

/* --------------------------------------------------------------- judge */

static void build(p10_book *b, uint32_t sleep_med, uint32_t sleep_p95,
                  uint32_t idle_med, uint32_t idle_p95,
                  uint32_t ble_med, uint32_t ble_p95, const char *why)
{
    p10_book_init(b);
    p10_reduction s = red(sleep_med, sleep_p95);
    p10_reduction i = red(idle_med, idle_p95);
    p10_reduction l = red(ble_med, ble_p95);
    p10_book_add(b, "sleep", &s, why);
    p10_book_add(b, "idle", &i, NULL);
    p10_book_add(b, "ble", &l, NULL);
}

static void test_judge(void)
{
    printf("\n-- the four comparisons, and what each one refuses\n");

    const char *WHY = "the USB-UART silicon leaks";
    p10_book b;
    p10_verdict v;

    /* Three distinct states, a repeat that held, and the sentence. */
    build(&b, 45000u, 52000u, 22000000u, 24000000u, 38000000u, 110000000u,
          WHY);
    p10_reduction again = red(45500u, 53000u);
    p10_book_add(&b, "sleep", &again, NULL);
    p10_judge(&b, &v);
    ok(v.ordered == 3u, "three distinct states are counted, not four, "
                        "because the repeat is the same state twice");
    ok(v.separated, "they are separated");
    ok(v.repeated && v.repeat_held, "the repeat held");
    ok(v.quietest_has_a_reason, "and the quietest carries its sentence");
    ok(p10_verdict_passed(&v), "so the run passes");

    /* Step 8, which is the one the lab exists for. Three captures of the
     * same thing, which is what a DUT still on its USB cable produces. */
    build(&b, 2400000u, 2600000u, 2450000u, 2620000u, 2430000u, 2610000u,
          WHY);
    p10_reduction u = red(2410000u, 2605000u);
    p10_book_add(&b, "sleep", &u, NULL);
    p10_judge(&b, &v);
    ok(!v.separated,
       "three traces that overlay are refused, which is the second power "
       "path this lab exists to catch");
    ok(!p10_verdict_passed(&v), "and the run does not pass");
    ok(b.e[v.overlap_lo].r.p95 >= b.e[v.overlap_hi].r.median,
       "and the pair it names really is a pair whose p95 is not below the "
       "next median");

    /* The boundary of the separation rule, from both sides, so that it is a
     * comparison and not a direction. */
    build(&b, 1000u, 2000u, 2000u, 3000u, 4000u, 5000u, WHY);
    p10_judge(&b, &v);
    ok(!v.separated,
       "a p95 exactly equal to the next median is not separated, because "
       "the rule is below and not at");
    build(&b, 1000u, 1999u, 2000u, 3000u, 4000u, 5000u, WHY);
    p10_judge(&b, &v);
    ok(v.separated, "one nanoamp below it is");

    /* Step 7. */
    build(&b, 45000u, 52000u, 22000000u, 24000000u, 38000000u, 110000000u,
          WHY);
    p10_judge(&b, &v);
    ok(!v.repeated && !p10_verdict_passed(&v),
       "a run with no second capture of any state is refused, because a run "
       "that cannot reproduce its own first number is not finished");

    build(&b, 45000u, 52000u, 22000000u, 24000000u, 38000000u, 110000000u,
          WHY);
    p10_reduction moved = red(80000u, 95000u);
    p10_book_add(&b, "sleep", &moved, NULL);
    p10_judge(&b, &v);
    ok(v.repeated && !v.repeat_held,
       "a repeat whose median sits above the first capture's p95 is refused");
    ok(!p10_verdict_passed(&v), "and that refusal fails the run");

    /* Step 9. */
    build(&b, 45000u, 52000u, 22000000u, 24000000u, 38000000u, 110000000u,
          NULL);
    p10_book_add(&b, "sleep", &again, NULL);
    p10_judge(&b, &v);
    ok(!v.quietest_has_a_reason && !p10_verdict_passed(&v),
       "a quietest state with no sentence beside it is refused, so its "
       "figure cannot be quoted later as a bare-module number");

    /* Two states are not three. */
    p10_book_init(&b);
    p10_reduction s2 = red(45000u, 52000u);
    p10_reduction i2 = red(22000000u, 24000000u);
    p10_book_add(&b, "sleep", &s2, WHY);
    p10_book_add(&b, "idle", &i2, NULL);
    p10_book_add(&b, "sleep", &again, NULL);
    p10_judge(&b, &v);
    ok(v.ordered == 2u && !p10_verdict_passed(&v),
       "two states are refused, because the chapter asks for three");

    /* A state with no samples at all. */
    p10_book_init(&b);
    p10_reduction empty;
    memset(&empty, 0, sizeof empty);
    p10_book_add(&b, "sleep", &s2, WHY);
    p10_book_add(&b, "idle", &i2, NULL);
    p10_book_add(&b, "ble", &empty, NULL);
    p10_judge(&b, &v);
    ok(!v.every_state_has_two_numbers,
       "a state carrying no samples has no median to write down, and is "
       "counted rather than passed over");
}

/* --------------------------------------------------------------- order */

static void test_order(void)
{
    printf("\n-- the order is measured, not read off the firmware's names\n");

    /* The names are deliberately wrong here: the state called "sleep" draws
     * the most. A program that trusted the name would put it first. */
    p10_book b;
    p10_book_init(&b);
    p10_reduction loud = red(90000000u, 110000000u);
    p10_reduction mid = red(22000000u, 24000000u);
    p10_reduction quiet = red(45000u, 52000u);
    p10_book_add(&b, "sleep", &loud, NULL);
    p10_book_add(&b, "idle", &mid, NULL);
    p10_book_add(&b, "ble", &quiet, "the USB-UART silicon leaks");

    p10_verdict v;
    p10_judge(&b, &v);
    ok(v.ordered == 3u, "three states");
    ok(strcmp(b.e[v.order[0]].name, "ble") == 0,
       "the quietest is the one that measured quietest, whatever it is "
       "called");
    ok(strcmp(b.e[v.order[2]].name, "sleep") == 0,
       "and the state named for deep sleep is last, because this bench "
       "measured it there");
    ok(v.quietest_has_a_reason,
       "and the sentence is required of the state that measured quietest, "
       "which is the figure that would be quoted as a bare-module number");
    ok(v.separated, "the three are still distinct, which the names do not "
                    "affect either");
}

static void cleanup(void)
{
    remove(TMP_CAP);
    remove(TMP_BOOK);
}

int main(void)
{
    printf("P10 suite: no instrument, no DUT, no USB cable, no trace\n");

    test_format();
    test_histogram();
    test_parse();
    test_book();
    test_judge();
    test_order();
    cleanup();

    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    if (g_failed > 0)
        return 1;
    printf("Nothing was measured. Every capture this suite reduced was one\n"
           "it wrote a moment earlier, and every quantile it checked was\n"
           "worked out from the samples rather than read off the program.\n");
    return 0;
}
