/* The lab book, and the eight items judged against it.
 *
 * Every field of the book has an absent state and absent is never read as
 * false. A rail nobody measured and a rail that measured wrong are
 * different facts, and a report that printed both as a failure would hide
 * the only one of them that can be fixed by walking back to the bench.
 */

#include "p05.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void p05_book_init(p05_book *b)
{
    if (b == NULL)
        return;
    memset(b, 0, sizeof *b);
    snprintf(b->instrument, sizeof b->instrument, "none");
}

p05_status p05_book_write(const p05_book *b, const char *path)
{
    if (b == NULL || path == NULL)
        return P05_E_ARG;
    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return P05_E_IO;

    int n = fprintf(fp,
        "# P05 lab book. The rails are what an instrument other than the\n"
        "# board under test read. A line that is absent means nobody took\n"
        "# that reading, which is not the same as a reading that failed.\n"
        "version=1\n"
        "date=%s\n"
        "instrument=%s\n",
        b->date, b->instrument);

    if (n >= 0 && b->have_rail_3v3)
        n = fprintf(fp, "rail_3v3_v=%.4f\n", b->rail_3v3_v);
    if (n >= 0 && b->have_rail_5v)
        n = fprintf(fp, "rail_5v_v=%.4f\n", b->rail_5v_v);
    if (n >= 0 && b->have_board)
        n = fprintf(fp, "board_address=%u\n", (unsigned)b->board_address);
    if (n >= 0 && b->have_header_clear)
        n = fprintf(fp, "header_clear=%d\n", b->header_clear ? 1 : 0);
    if (n >= 0 && b->have_survived_reboot)
        n = fprintf(fp, "survived_reboot=%d\n", b->survived_reboot ? 1 : 0);
    if (n >= 0 && b->have_second_run)
        n = fprintf(fp, "second_run_clean=%d\n", b->second_run_clean ? 1 : 0);

    /* The four commands the volume asks for beside any recorded number. A
     * board that boots is not evidence that the image you built is running
     * on it. */
    if (n >= 0)
        n = fprintf(fp,
            "os_release=%s\n"
            "kernel=%s\n"
            "model=%s\n"
            "root_fs=%s\n",
            b->os_release, b->kernel, b->model, b->root_fs);

    if (fclose(fp) != 0 || n < 0)
        return P05_E_IO;
    return P05_OK;
}

static void trim(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' '))
        s[--n] = '\0';
}

p05_status p05_book_read(p05_book *b, const char *path)
{
    if (b == NULL || path == NULL)
        return P05_E_ARG;
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return P05_E_IO;

    p05_book_init(b);
    char line[320];
    bool version_seen = false;

    while (fgets(line, (int)sizeof line, fp) != NULL) {
        if (line[0] == '#')
            continue;
        trim(line);
        char *eq = strchr(line, '=');
        if (eq == NULL)
            continue;
        *eq = '\0';
        const char *k = line;
        const char *v = eq + 1;

        if (strcmp(k, "version") == 0) {
            if (atoi(v) != 1) {
                fclose(fp);
                return P05_E_FORMAT;
            }
            version_seen = true;
        }
        else if (strcmp(k, "rail_3v3_v") == 0) {
            b->rail_3v3_v = atof(v);
            b->have_rail_3v3 = true;
        }
        else if (strcmp(k, "rail_5v_v") == 0) {
            b->rail_5v_v = atof(v);
            b->have_rail_5v = true;
        }
        else if (strcmp(k, "board_address") == 0) {
            b->board_address = (uint8_t)atoi(v);
            b->have_board = true;
        }
        else if (strcmp(k, "header_clear") == 0) {
            b->header_clear = (atoi(v) != 0);
            b->have_header_clear = true;
        }
        else if (strcmp(k, "survived_reboot") == 0) {
            b->survived_reboot = (atoi(v) != 0);
            b->have_survived_reboot = true;
        }
        else if (strcmp(k, "second_run_clean") == 0) {
            b->second_run_clean = (atoi(v) != 0);
            b->have_second_run = true;
        }
        else if (strcmp(k, "instrument") == 0)
            snprintf(b->instrument, sizeof b->instrument, "%s", v);
        else if (strcmp(k, "date") == 0)
            snprintf(b->date, sizeof b->date, "%s", v);
        else if (strcmp(k, "os_release") == 0)
            snprintf(b->os_release, sizeof b->os_release, "%s", v);
        else if (strcmp(k, "kernel") == 0)
            snprintf(b->kernel, sizeof b->kernel, "%s", v);
        else if (strcmp(k, "model") == 0)
            snprintf(b->model, sizeof b->model, "%s", v);
        else if (strcmp(k, "root_fs") == 0)
            snprintf(b->root_fs, sizeof b->root_fs, "%s", v);
    }

    if (fclose(fp) != 0)
        return P05_E_IO;
    return version_seen ? P05_OK : P05_E_FORMAT;
}

/* ---------------------------------------------------------------- the gate */

static p05_item near_enough(double got, double want, double tol)
{
    return (fabs(got - want) <= tol) ? P05_ITEM_PASS : P05_ITEM_FAIL;
}

static p05_item recorded_true(bool have, bool value)
{
    if (!have)
        return P05_ITEM_UNRECORDED;
    return value ? P05_ITEM_PASS : P05_ITEM_FAIL;
}

p05_status p05_judge(const p05_scan *s, const p05_book *b, p05_verdict *v)
{
    if (s == NULL || b == NULL || v == NULL)
        return P05_E_ARG;
    memset(v, 0, sizeof *v);

    if (s->rows == 0 || s->channels < 2)
        return P05_E_FORMAT;

    v->ch0_mean = s->mean[0];
    v->ch1_mean = s->mean[1];

    v->ch0_on_rail = near_enough(s->mean[0], P05_RAIL_3V3, P05_RAIL_TOLERANCE);
    v->ch1_near_5v = near_enough(s->mean[1], P05_RAIL_5V, P05_CH1_TOLERANCE);

    v->sample_count = (s->rows == P05_EXPECT_ROWS && s->holes == 0)
                    ? P05_ITEM_PASS : P05_ITEM_FAIL;
    v->index_monotonic = (s->starts_at_zero && s->repeats == 0 &&
                          s->backwards == 0 && s->holes == 0)
                       ? P05_ITEM_PASS : P05_ITEM_FAIL;

    v->board_named = b->have_board
                   ? (b->board_address == 0 ? P05_ITEM_PASS : P05_ITEM_FAIL)
                   : P05_ITEM_UNRECORDED;
    v->header_clear = recorded_true(b->have_header_clear && b->have_survived_reboot,
                                    b->header_clear && b->survived_reboot);
    v->second_run_clean = recorded_true(b->have_second_run, b->second_run_clean);

    /* The item this bench cannot buy with time. An independent instrument
     * has to disagree or agree with the board, and the board cannot play
     * both parts. */
    if (!b->have_rail_3v3) {
        v->instrument_agrees = P05_ITEM_IMPOSSIBLE;
    } else {
        v->instrument_difference = fabs(b->rail_3v3_v - s->mean[0]);
        v->instrument_agrees = (v->instrument_difference <= P05_RAIL_TOLERANCE)
                             ? P05_ITEM_PASS : P05_ITEM_FAIL;
    }
    return P05_OK;
}

static const char *word(p05_item i)
{
    switch (i) {
    case P05_ITEM_PASS:       return "pass";
    case P05_ITEM_FAIL:       return "FAIL";
    case P05_ITEM_UNRECORDED: return "none";
    case P05_ITEM_IMPOSSIBLE: return "n/a ";
    }
    return "????";
}

static bool all(const p05_verdict *v, bool allow_impossible)
{
    const p05_item items[] = {
        v->ch0_on_rail, v->sample_count, v->board_named, v->index_monotonic,
        v->header_clear, v->ch1_near_5v, v->instrument_agrees,
        v->second_run_clean
    };
    for (size_t i = 0; i < sizeof items / sizeof items[0]; i++) {
        if (items[i] == P05_ITEM_PASS)
            continue;
        if (allow_impossible && items[i] == P05_ITEM_IMPOSSIBLE)
            continue;
        return false;
    }
    return true;
}

bool p05_verdict_passed(const p05_verdict *v)
{
    return (v != NULL) && all(v, false);
}

bool p05_verdict_passed_what_is_possible(const p05_verdict *v)
{
    return (v != NULL) && all(v, true);
}

void p05_verdict_print(const p05_verdict *v, FILE *out)
{
    if (v == NULL || out == NULL)
        return;

    fprintf(out, "%s  ch0 on the 3.3 V rail: %.4f V measured from the file\n",
            word(v->ch0_on_rail), v->ch0_mean);
    fprintf(out, "%s  ch1 near the 5 V rail: %.4f V\n",
            word(v->ch1_near_5v), v->ch1_mean);
    fprintf(out, "%s  one second at 10 kS/s with no holes\n", word(v->sample_count));
    fprintf(out, "%s  the index is monotonic from zero with no repeats\n",
            word(v->index_monotonic));
    fprintf(out, "%s  one board, at address zero, recorded by the operator\n",
            word(v->board_named));
    fprintf(out, "%s  nothing else on the header, and the board still there "
                 "after a reboot\n", word(v->header_clear));
    fprintf(out, "%s  a second run started cleanly after stop and cleanup\n",
            word(v->second_run_clean));

    if (v->instrument_agrees == P05_ITEM_IMPOSSIBLE) {
        fprintf(out, "n/a   an instrument other than the board agrees with "
                     "ch0: NOT RECORDED, and not recordable here\n");
    } else {
        fprintf(out, "%s  an instrument other than the board agrees with ch0, "
                     "within %.4f V\n",
                word(v->instrument_agrees), v->instrument_difference);
    }

    fprintf(out, "\n");
    if (p05_verdict_passed(v)) {
        fprintf(out, "Every item of the acceptance test is satisfied.\n");
        return;
    }
    if (p05_verdict_passed_what_is_possible(v)) {
        fprintf(out,
            "Every item this bench can settle is satisfied, and one cannot\n"
            "be settled here at all. The acceptance test asks that a second\n"
            "instrument agree with the board, and there is no voltmeter in\n"
            "this room. The board cannot stand in for it: the whole point of\n"
            "that item is an independent reading, and a board checking\n"
            "itself is not one.\n"
            "\n"
            "So this lab stops one item short of a pass, and no amount of\n"
            "bench time moves it. A cheap multimeter does, which makes it the\n"
            "single purchase that unblocks the most acceptance tests in this\n"
            "volume.\n");
        return;
    }
    fprintf(out,
        "The acceptance test is not met. Items marked none were never\n"
        "written down, which an afternoon at the bench fixes, and items\n"
        "marked FAIL were measured and disagreed.\n");
}
