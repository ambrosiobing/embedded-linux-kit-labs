/* The suite. No framework, no hardware, no serial port, no network.
 *
 * Six groups, kept apart because they fail for different reasons:
 *
 *   reader         whether a line that does not fit is refused rather than
 *                  split into a sample nobody sent
 *   parser         whether the one field this program reads is read strictly
 *   accounting     whether gaps, duplicates and reorders are counted, and
 *                  whether the counter returning to zero is one step forward
 *   synthesis      whether the same request produces the same stream, and
 *                  whether an injected fault is the only difference
 *   end to end     whether synth, sink and count agree about one file, and
 *                  whether a report survives a write and a read
 *   two faults     whether loss and rate stay separate, which is this lab's
 *                  own claim and the only one worth publishing
 *
 * Nothing here sleeps. The timing half of the lab is tested by constructing
 * the run a real capture would have produced and applying the gate to it: a
 * suite that waits ten seconds to check a ten second rule is a suite people
 * stop running.
 */

#include "../src/p18.h"

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

static void ok_near(double got, double want, double tol, const char *what)
{
    g_checks++;
    if (fabs(got - want) <= tol) {
        printf("ok   %s (%.6g)\n", what, got);
    } else {
        g_failed++;
        printf("FAIL %s: got %.6g, wanted %.6g within %.6g\n", what, got, want, tol);
    }
}

/* ---------------------------------------------------------------- helpers */

static const char *TMP_STREAM = "test_p18.stream";
static const char *TMP_OTHER  = "test_p18_other.stream";
static const char *TMP_CAP    = "test_p18.capture";
static const char *TMP_REPORT = "test_p18.p18";

static bool write_text(const char *path, const char *text)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return false;
    size_t n = strlen(text);
    size_t w = fwrite(text, 1, n, fp);
    return fclose(fp) == 0 && w == n;
}

static bool file_bytes(const char *path, char *buf, size_t cap, size_t *len)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return false;
    size_t n = fread(buf, 1, cap - 1, fp);
    buf[n] = '\0';
    *len = n;
    return fclose(fp) == 0;
}

static uint32_t count_lines(const char *path)
{
    p18_reader r;
    if (p18_reader_open(&r, path) != P18_OK)
        return 0;
    uint32_t n = 0;
    for (;;) {
        const char *line = NULL;
        size_t len = 0;
        if (p18_reader_line(&r, &line, &len) != P18_OK)
            break;
        n++;
    }
    p18_reader_close(&r);
    return n;
}

/* ---------------------------------------------------------------- groups */

static void test_reader(void)
{
    printf("\n-- reader\n");

    ok(write_text(TMP_STREAM, "1,a\n2,b\n3,c\n"), "a three line stream is written");
    ok(count_lines(TMP_STREAM) == 3, "three lines are read back");

    ok(write_text(TMP_STREAM, "1,a\n\n\n2,b\n"), "a stream with blank lines is written");
    ok(count_lines(TMP_STREAM) == 2, "blank lines are skipped, not counted as samples");

    ok(write_text(TMP_STREAM, "1,a\r\n2,b\r\n"), "a stream with carriage returns is written");
    ok(count_lines(TMP_STREAM) == 2, "a carriage return is stripped with the newline");

    ok(write_text(TMP_STREAM, "1,a\n2,b"), "a stream whose last line has no newline");
    ok(count_lines(TMP_STREAM) == 2, "the unterminated final line is still a line");

    /* The one that matters. A line longer than the buffer must not come back
     * as two lines, because the second one would parse as a sequence number
     * that was never sent. */
    {
        char big[P18_LINE_MAX + 128];
        memset(big, 'x', sizeof big);
        char *p = big;
        int n = snprintf(p, sizeof big, "7,");
        memset(big + n, 'x', sizeof big - (size_t)n - 2);
        big[sizeof big - 2] = '\n';
        big[sizeof big - 1] = '\0';

        FILE *fp = fopen(TMP_STREAM, "wb");
        ok(fp != NULL, "a stream with one over-long line is opened");
        if (fp != NULL) {
            fprintf(fp, "1,a\n");
            fputs(big, fp);
            fprintf(fp, "9,b\n");
            fclose(fp);
        }

        p18_reader r;
        ok(p18_reader_open(&r, TMP_STREAM) == P18_OK, "it opens");
        uint32_t seen = 0;
        uint32_t seqs[8] = {0};
        for (;;) {
            const char *line = NULL;
            size_t len = 0;
            if (p18_reader_line(&r, &line, &len) != P18_OK)
                break;
            uint32_t seq = 0;
            if (p18_parse_seq(line, &seq) == P18_OK && seen < 8)
                seqs[seen++] = seq;
        }
        ok(r.long_lines == 1, "the over-long line is counted once");
        ok(seen == 2, "it produced no extra sample, and the two good lines remain");
        ok(seen == 2 && seqs[0] == 1 && seqs[1] == 9,
           "the sequence numbers either side of it are 1 and 9");
        p18_reader_close(&r);
    }
}

static void test_parser(void)
{
    printf("\n-- parser\n");

    uint32_t seq = 0;
    ok(p18_parse_seq("17,1,2,3", &seq) == P18_OK && seq == 17,
       "a sequence number followed by a payload is read");
    ok(p18_parse_seq("42", &seq) == P18_OK && seq == 42,
       "a sequence number alone is read");
    ok(p18_parse_seq("4294967295,x", &seq) == P18_OK && seq == 4294967295u,
       "the largest 32 bit sequence number is read");
    ok(p18_parse_seq("4294967296,x", &seq) == P18_E_RANGE,
       "one more than that is refused rather than wrapped");
    ok(p18_parse_seq("-1,x", &seq) == P18_E_FORMAT,
       "a negative number is refused rather than read as four billion");
    ok(p18_parse_seq("x,1", &seq) == P18_E_FORMAT,
       "a line with no leading number is refused");
    ok(p18_parse_seq("", &seq) == P18_E_FORMAT,
       "an empty line is refused");
    ok(p18_parse_seq("12x,1", &seq) == P18_E_FORMAT,
       "digits glued to something else are refused, not truncated");
    ok(p18_parse_seq(" 12,1", &seq) == P18_E_FORMAT,
       "a leading space is refused: the field starts where the line starts");
}

static void test_accounting(void)
{
    printf("\n-- accounting\n");

    seqtrack t;
    seqtrack_init(&t);
    for (uint32_t i = 0; i < 100; i++)
        seqtrack_see(&t, i);
    ok(t.gaps == 0 && t.duplicates == 0 && t.reorders == 0,
       "one hundred samples in order carry no fault");
    ok(t.expected == 100 && t.received == 100, "one hundred expected, one hundred received");

    seqtrack_init(&t);
    seqtrack_see(&t, 0);
    seqtrack_see(&t, 5);
    ok(t.gaps == 4, "a jump from 0 to 5 reports four missing");

    seqtrack_init(&t);
    seqtrack_see(&t, 3);
    seqtrack_see(&t, 3);
    ok(t.duplicates == 1 && t.gaps == 0, "a repeat is a duplicate and not a gap");

    seqtrack_init(&t);
    seqtrack_see(&t, 3);
    seqtrack_see(&t, 4);
    seqtrack_see(&t, 3);
    ok(t.reorders == 1 && t.gaps == 0, "a late arrival is a reorder and not a gap");

    /* The pitfall the chapter names, settled in the arithmetic rather than in
     * a sentence. At one kilohertz the counter reaches this after about four
     * and a half weeks, which is a field deployment rather than a bench run. */
    seqtrack_init(&t);
    seqtrack_see(&t, 0xFFFFFFFDu);
    seqtrack_see(&t, 0xFFFFFFFEu);
    seqtrack_see(&t, 0xFFFFFFFFu);
    seqtrack_see(&t, 0u);
    seqtrack_see(&t, 1u);
    ok(t.gaps == 0 && t.reorders == 0,
       "the counter returning to zero is one step forward, not four billion back");
    ok(t.expected == 5, "five expected across the wrap");

    seqtrack_init(&t);
    seqtrack_see(&t, 0xFFFFFFFEu);
    seqtrack_see(&t, 2u);
    ok(t.gaps == 3, "a hole straddling the wrap is three missing");

    seqtrack_init(&t);
    seqtrack_see(&t, 10u);
    seqtrack_see(&t, 9u);
    ok(seqtrack_behind(9u, 10u), "nine sits behind ten");
    ok(!seqtrack_behind(0u, 0xFFFFFFFFu), "zero sits ahead of the largest value");
    ok(seqtrack_behind(0xFFFFFFFFu, 0u), "and the largest value sits behind zero");
}

static void test_synthesis(void)
{
    printf("\n-- synthesis\n");

    p18_synth s;
    p18_synth_init(&s);
    s.samples = 50;
    ok(p18_synth_write(&s, TMP_STREAM) == P18_OK, "a clean stream is written");
    ok(p18_synth_write(&s, TMP_OTHER) == P18_OK, "and written again");

    static char a[65536];
    static char b[65536];
    size_t na = 0, nb = 0;
    ok(file_bytes(TMP_STREAM, a, sizeof a, &na), "the first is read back");
    ok(file_bytes(TMP_OTHER, b, sizeof b, &nb), "the second is read back");
    ok(na == nb && memcmp(a, b, na) == 0,
       "the same request twice gives the same bytes, to the byte");

    p18_synth_init(&s);
    s.samples = 50;
    ok(p18_synth_add_fault(&s, P18_FAULT_DROP, 20) == P18_OK, "one sample is withheld");
    ok(p18_synth_write(&s, TMP_OTHER) == P18_OK, "the holed stream is written");
    ok(count_lines(TMP_OTHER) == 49, "it is one line shorter and no more");

    static char holed[65536];
    size_t nh = 0;
    ok(file_bytes(TMP_OTHER, holed, sizeof holed, &nh), "the holed stream is read back");
    ok(strstr(holed, "\n20,") == NULL, "sample 20 is absent from it");
    ok(strstr(b, "\n20,") != NULL, "and was present in the same stream without the fault");

    p18_run run;
    p18_run_init(&run, 0.0);
    ok(p18_run_capture(TMP_OTHER, NULL, 0.0, 0, &run) == P18_OK, "it is counted");
    ok(run.seq.gaps == 1, "exactly one sample is reported missing");
    ok(run.samples == 49, "forty nine arrived");

    p18_synth_init(&s);
    s.samples = 20;
    ok(p18_synth_add_fault(&s, P18_FAULT_DUP, 5) == P18_OK, "one sample is repeated");
    ok(p18_synth_add_fault(&s, P18_FAULT_SWAP, 10) == P18_OK, "one pair is swapped");
    ok(p18_synth_add_fault(&s, P18_FAULT_GARBLE, 15) == P18_OK, "one line is garbled");
    ok(p18_synth_write(&s, TMP_OTHER) == P18_OK, "the faulty stream is written");

    p18_run_init(&run, 0.0);
    ok(p18_run_capture(TMP_OTHER, NULL, 0.0, 0, &run) == P18_OK, "it is counted");
    ok(run.seq.duplicates == 1, "the repeat is counted as one duplicate");
    ok(run.seq.reorders == 1, "the swap is counted as one reorder");
    ok(run.malformed == 1, "the garbled line is counted as one malformed line");
    /* Two gaps, and the second one is worth naming. The garbled line leaves a
     * hole because nothing numbered it. The swapped pair leaves one as well:
     * when the later sample arrived first, the one it overtook was counted
     * missing, and its arrival afterwards is recorded as a reorder without
     * the gap being taken back. seqtrack.h says why that is deliberate. */
    ok(run.seq.gaps == 2, "two gaps: one from the garbled line, one from the overtaking");
    ok(run.samples == 20, "twenty numbered lines were kept, the duplicate included");
    ok(run.seq.expected == 20, "and the span from the first to the last is twenty");
}

static void test_end_to_end(void)
{
    printf("\n-- end to end\n");

    p18_synth s;
    p18_synth_init(&s);
    s.samples = 500;
    ok(p18_synth_write(&s, TMP_STREAM) == P18_OK, "a five hundred sample stream");

    p18_run run;
    p18_run_init(&run, P18_RATE_DEFAULT);
    ok(p18_run_capture(TMP_STREAM, TMP_CAP, 0.0, 0, &run) == P18_OK, "it is recorded");
    ok(run.samples == 500, "five hundred samples are kept");
    ok(run.seq.gaps == 0, "with no gaps");
    ok(!run.rate_measured,
       "and no rate is claimed, because a file is not a stream in real time");

    p18_run again;
    p18_run_init(&again, 0.0);
    ok(p18_run_capture(TMP_CAP, NULL, 0.0, 0, &again) == P18_OK, "the capture is counted");
    ok(again.samples == run.samples && again.seq.gaps == run.seq.gaps,
       "the capture and the stream agree about what arrived");

    ok(p18_report_write(&run, TMP_REPORT) == P18_OK, "a report is written");
    p18_run back;
    ok(p18_report_read(&back, TMP_REPORT) == P18_OK, "and read");
    ok(back.samples == run.samples, "samples survive the round trip");
    ok(back.seq.gaps == run.seq.gaps, "gaps survive the round trip");
    ok(back.seq.first_seq == run.seq.first_seq && back.seq.last_seq == run.seq.last_seq,
       "the first and last sequence numbers survive the round trip");
    ok_near(back.nominal_rate_hz, run.nominal_rate_hz, 1e-6,
            "the nominal rate survives the round trip");

    p18_run_init(&run, 1000.0);
    ok(p18_run_capture(TMP_STREAM, NULL, 0.0, 100, &run) == P18_OK, "a limited run");
    ok(run.samples == 100 && run.truncated,
       "it stops at one hundred and says the stream had more to give");
}

static void test_two_faults(void)
{
    printf("\n-- two faults, kept apart\n");

    /* Both runs are constructed rather than recorded, because what is under
     * test is the gate's arithmetic and not the clock. The numbers are the
     * ones a real ten second capture at one kilohertz would have produced. */
    p18_gate g;
    p18_gate_init(&g);

    p18_run good;
    p18_run_init(&good, 1000.0);
    good.samples = 10000;
    good.seq.started = true;
    good.seq.received = 10000;
    good.seq.expected = 10000;
    good.elapsed_s = 10.0;
    good.observed_rate_hz = 1000.0;
    good.rate_measured = true;

    p18_verdict v;
    ok(p18_gate_apply(&g, &good, &v) == P18_OK, "a clean run is gated");
    ok(p18_verdict_passed(&v), "and it passes on all three counts");

    /* The chapter's own threshold, at the threshold. Ten samples missing in
     * ten thousand is a tenth of a per cent, which the acceptance test allows
     * rather than refuses, and a limit that rejects the value it names is the
     * usual way a threshold turns into an argument. */
    p18_run edge = good;
    edge.samples = 9990;
    edge.seq.received = 9990;
    edge.seq.gaps = 10;
    edge.observed_rate_hz = 999.0;
    ok(p18_gate_apply(&g, &edge, &v) == P18_OK, "a run at exactly the limit is gated");
    ok_near(v.loss_pct, 0.1, 1e-9, "its loss is a tenth of a per cent");
    ok(v.loss_ok && p18_verdict_passed(&v), "the limit is inclusive, so it passes");

    /* Fault one: the samples that did arrive were on time, and twenty of them
     * did not arrive. */
    p18_run lossy = good;
    lossy.samples = 9980;
    lossy.seq.received = 9980;
    lossy.seq.gaps = 20;
    lossy.observed_rate_hz = 998.0;
    ok(p18_gate_apply(&g, &lossy, &v) == P18_OK, "a lossy run is gated");
    ok(!v.loss_ok, "the loss check fails");
    ok(v.rate_ok, "the rate check passes, because the rate was never the problem");
    ok_near(v.loss_pct, 0.2, 1e-9, "the loss is two tenths of a per cent");

    /* Fault two: nothing was lost at all, and the stream ran at half the rate
     * it was supposed to. The chapter names this as a different fault, and
     * here it reads as one. */
    p18_run slow = good;
    slow.samples = 5000;
    slow.seq.received = 5000;
    slow.seq.expected = 5000;
    slow.seq.gaps = 0;
    slow.observed_rate_hz = 500.0;
    ok(p18_gate_apply(&g, &slow, &v) == P18_OK, "a slow run is gated");
    ok(v.loss_ok, "the loss check passes, because nothing was lost");
    ok(!v.rate_ok, "the rate check fails");
    ok_near(v.rate_ratio, 0.5, 1e-9, "the stream ran at half its nominal rate");

    /* A run whose rate was never measured is not a run whose rate passed, and
     * the difference is one flag rather than an assumption. */
    p18_run unmeasured = good;
    unmeasured.rate_measured = false;
    ok(p18_gate_apply(&g, &unmeasured, &v) == P18_OK, "an unmeasured run is gated");
    ok(!v.rate_ok, "by default an unmeasured rate does not pass");
    g.require_rate = false;
    ok(p18_gate_apply(&g, &unmeasured, &v) == P18_OK, "and gated again, with the flag");
    ok(v.rate_ok, "with the flag set, it is allowed through knowingly");
    g.require_rate = true;

    /* A capture that threw lines away is not a capture of the stream, whatever
     * its loss figure says. */
    p18_run ragged = good;
    ragged.long_lines = 1;
    ok(p18_gate_apply(&g, &ragged, &v) == P18_OK, "a run with a refused line is gated");
    ok(v.loss_ok && v.rate_ok, "loss and rate both pass");
    ok(!v.integrity_ok, "and the run still fails, on integrity");
    ok(!p18_verdict_passed(&v), "one failed check is a failed acceptance test");
}

static void cleanup(void)
{
    remove(TMP_STREAM);
    remove(TMP_OTHER);
    remove(TMP_CAP);
    remove(TMP_REPORT);
}

int main(void)
{
    printf("P18 suite: no hardware, no serial port, no network\n");

    test_reader();
    test_parser();
    test_accounting();
    test_synthesis();
    test_end_to_end();
    test_two_faults();
    cleanup();

    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    if (g_failed > 0)
        return 1;
    printf("Nothing here was measured on a probe. Every number came from a\n"
           "file this suite wrote a moment earlier.\n");
    return 0;
}
