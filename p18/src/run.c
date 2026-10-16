/* The capture loop: read lines, account for the sequence numbers, keep the
 * lines, and measure how long it all took.
 *
 * The clock is CLOCK_MONOTONIC. A wall clock can step backwards, and a time
 * synchronisation in the middle of a ten second capture would put a negative
 * interval into the one measurement this loop makes.
 */
#define _POSIX_C_SOURCE 200809L

#include "p18.h"

#include <string.h>
#include <time.h>

static double now_s(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0.0;
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

void p18_run_init(p18_run *run, double nominal_rate_hz)
{
    if (run == NULL)
        return;
    memset(run, 0, sizeof *run);
    seqtrack_init(&run->seq);
    run->nominal_rate_hz = nominal_rate_hz;
}

double p18_loss_percent(const p18_run *run)
{
    if (run == NULL || run->seq.expected == 0)
        return 0.0;
    return 100.0 * (double)run->seq.gaps / (double)run->seq.expected;
}

p18_status p18_run_capture(const char *source, const char *out,
                           double seconds, uint32_t max_samples,
                           p18_run *run)
{
    if (source == NULL || run == NULL)
        return P18_E_ARG;

    p18_reader r;
    p18_status s = p18_reader_open(&r, source);
    if (s != P18_OK)
        return s;

    FILE *fp = NULL;
    if (out != NULL) {
        fp = fopen(out, "wb");
        if (fp == NULL) {
            p18_reader_close(&r);
            return P18_E_IO;
        }
    }

    const double start = now_s();
    p18_status st = P18_OK;

    for (;;) {
        const char *line = NULL;
        size_t len = 0;
        p18_status rs = p18_reader_line(&r, &line, &len);
        if (rs == P18_E_EOF)
            break;
        if (rs != P18_OK) {
            st = rs;
            break;
        }

        uint32_t seq = 0;
        if (p18_parse_seq(line, &seq) != P18_OK) {
            /* Counted, not kept, and not repaired. A line with no sequence
             * number cannot be placed in the stream, and guessing where it
             * belongs would put a sample into the record that the firmware
             * never numbered. */
            run->malformed++;
            continue;
        }

        seqtrack_see(&run->seq, seq);
        run->samples++;

        if (fp != NULL && fprintf(fp, "%s\n", line) < 0) {
            st = P18_E_IO;
            break;
        }

        if (max_samples != 0 && run->samples >= max_samples) {
            run->truncated = true;
            break;
        }
        if (seconds > 0.0 && now_s() - start >= seconds) {
            run->deadline_hit = true;
            break;
        }
    }

    const double end = now_s();
    run->elapsed_s = end - start;
    run->long_lines = r.long_lines;
    run->blank_lines = r.blank_lines;

    if (run->elapsed_s > 0.0 && run->samples > 0)
        run->observed_rate_hz = (double)run->samples / run->elapsed_s;
    else
        run->observed_rate_hz = 0.0;

    /* Whether a rate may be claimed at all. A capture file on a disk is read
     * in milliseconds, and samples divided by that is a measurement of this
     * program's reading speed, not of the probe. Saying so is the difference
     * between a number and a number that means something. */
    run->rate_measured = false;
    if (run->samples > 0 && run->nominal_rate_hz > 0.0 && run->elapsed_s > 0.0) {
        double should_take = (double)run->samples / run->nominal_rate_hz;
        if (run->elapsed_s >= P18_REALTIME_FRACTION * should_take)
            run->rate_measured = true;
    }

    if (fp != NULL && fclose(fp) != 0 && st == P18_OK)
        st = P18_E_IO;
    p18_reader_close(&r);
    return st;
}
