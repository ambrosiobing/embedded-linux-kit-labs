/* P10's energy bench, on the command line.
 *
 * The chapter's steps map onto the subcommands one for one. `scene` writes a
 * capture that did not happen, so the rest can be exercised with no PPK2 in
 * the room. `reduce` turns one capture into the two numbers step 6 asks for
 * and appends them to the lab book. `report` prints the book the way a
 * reader would copy it out. `judge` applies steps 7, 8 and 9 and exits 2
 * when the run is refused.
 *
 * `judge` is the one that matters. The other three are arithmetic and
 * transcription; this is the one that says whether the DUT had a second
 * power path, which is the whole experiment.
 */
#define _POSIX_C_SOURCE 200809L

#include "p10.h"

#include <stdlib.h>
#include <string.h>

#define EXIT_USAGE   1
#define EXIT_REFUSED 2

static const char *USAGE =
    "usage: p10 <command> [options]\n"
    "\n"
    "  scene   --kind K [--samples N] [--out FILE]\n"
    "          K is ramp, sleep, idle, ble or usb. usb is what a DUT that is\n"
    "          still plugged into its own USB cable looks like through the\n"
    "          instrument, and writing it three times is how this lab fails.\n"
    "\n"
    "  reduce  --source FILE --state NAME [--book FILE] [--column N]\n"
    "          [--why TEXT]\n"
    "          two passes over the capture, a median and a p95 appended to\n"
    "          the book. --why records the sentence step 9 asks for.\n"
    "\n"
    "  report  --book FILE\n"
    "  judge   --book FILE          exits 2 when the run is refused\n";

/* ---------------------------------------------------------------- report */

static void print_entry(const p10_entry *e)
{
    char med[24];
    char p95[24];
    char lo[24];
    char hi[24];
    char bucket[24];
    if (!p10_format_current(med, sizeof med, e->r.median) ||
        !p10_format_current(p95, sizeof p95, e->r.p95) ||
        !p10_format_current(lo, sizeof lo, e->r.lo) ||
        !p10_format_current(hi, sizeof hi, e->r.hi) ||
        !p10_format_current(bucket, sizeof bucket, e->r.bucket))
        return;

    printf("  %-12s median %-11s p95 %-11s\n", e->name, med, p95);
    printf("  %-12s range  %s to %s, %lu samples, %lu refused\n",
           "", lo, hi, (unsigned long)e->r.n, (unsigned long)e->r.refused);
    printf("  %-12s these two figures are good to %s, which is the bucket\n",
           "", bucket);
    if (e->reason[0] != '\0')
        printf("  %-12s why: %s\n", "", e->reason);
}

static int cmd_report(const char *book_path)
{
    p10_book b;
    p10_status s = p10_book_read(&b, book_path);
    if (s != P10_OK) {
        fprintf(stderr, "p10: %s\n", p10_strstatus(s));
        return EXIT_REFUSED;
    }
    printf("The lab book, %u entr%s.\n\n", b.n, (b.n == 1u) ? "y" : "ies");
    for (unsigned i = 0; i < b.n; i++) {
        print_entry(&b.e[i]);
        putchar('\n');
    }
    printf("A median says what the state normally costs and a p95 says what\n"
           "it costs when the radio is busy. Neither is a screenshot of a\n"
           "trace, which is the point of step 6.\n");
    return 0;
}

/* ----------------------------------------------------------------- judge */

static int cmd_judge(const char *book_path)
{
    p10_book b;
    p10_status s = p10_book_read(&b, book_path);
    if (s != P10_OK) {
        fprintf(stderr, "p10: %s\n", p10_strstatus(s));
        return EXIT_REFUSED;
    }

    p10_verdict v;
    p10_judge(&b, &v);

    printf("%u state%s, in the order the bench measured rather than the\n"
           "order the firmware was named in:\n\n",
           v.ordered, (v.ordered == 1u) ? "" : "s");
    for (unsigned i = 0; i < v.ordered; i++) {
        const p10_entry *e = &b.e[v.order[i]];
        char med[24];
        char p95[24];
        if (p10_format_current(med, sizeof med, e->r.median) &&
            p10_format_current(p95, sizeof p95, e->r.p95))
            printf("  %u. %-12s median %-11s p95 %s\n", i + 1u, e->name, med,
                   p95);
    }

    putchar('\n');
    if (v.ordered < 3u)
        printf("REFUSED: the chapter asks for three firmware states and this\n"
               "book has %u.\n", v.ordered);

    if (!v.every_state_has_two_numbers)
        printf("REFUSED: a state in this book carries no samples, so it has\n"
               "no median and no p95 to write down.\n");

    if (v.separated) {
        printf("The states are distinct: each one's p95 sits below the next\n"
               "one's median, so ninety five per cent of the quieter state is\n"
               "below what the busier one typically draws.\n");
    } else if (v.ordered >= 2u) {
        const p10_entry *lo = &b.e[v.overlap_lo];
        const p10_entry *hi = &b.e[v.overlap_hi];
        char a[24];
        char c[24];
        if (p10_format_current(a, sizeof a, lo->r.p95) &&
            p10_format_current(c, sizeof c, hi->r.median))
            printf("REFUSED: %s and %s overlay. %s's p95 is %s and %s's\n"
                   "median is %s, so the quieter trace is not below the\n"
                   "busier one.\n"
                   "\n"
                   "Before anything else, check the DUT's own USB cable. A\n"
                   "second power path makes every number in this run an\n"
                   "unknown fraction of the truth, and that is the first\n"
                   "thing this looks like. The cause is the cable, not the\n"
                   "instrument.\n",
                   lo->name, hi->name, lo->name, a, hi->name, c);
    }

    putchar('\n');
    if (!v.repeated) {
        printf("REFUSED: no state was captured twice, so step 7 was not run.\n"
               "A run that cannot reproduce its own first number is not\n"
               "finished, whatever the other states reported.\n");
    } else if (v.repeat_held) {
        printf("The repeat held: %s was captured twice and neither median\n"
               "moved past the other's p95.\n", b.e[v.repeat_a].name);
    } else {
        printf("REFUSED: %s was captured twice and the median moved past the\n"
               "other capture's p95. Something on the bench moved with it: a\n"
               "lead, a ground, or the state itself.\n", b.e[v.repeat_a].name);
    }

    putchar('\n');
    if (v.quietest_has_a_reason) {
        printf("The quietest state carries its sentence, which is what keeps\n"
               "its figure from being quoted later as a bare-module number.\n");
    } else if (v.ordered > 0u) {
        printf("REFUSED: %s is the quietest state and carries no sentence\n"
               "saying why. On a dev-board that figure is tens of microamps\n"
               "to low milliamps because the USB-UART silicon leaks, and\n"
               "without that written beside it the number will be quoted as\n"
               "a bare-module deep-sleep current. Record it with --why.\n",
               b.e[v.order[0]].name);
    }

    putchar('\n');
    printf("Not decided here: whether these are the right figures for this\n"
           "board. The first two acceptance items ask whether deep-sleep is\n"
           "tens of microamps to low milliamps and whether the active radio\n"
           "is tens of milliamps, and those are judgements against a part,\n"
           "not against a capture.\n");

    if (!p10_verdict_passed(&v))
        return EXIT_REFUSED;

    printf("\nEvery item this program can decide is satisfied.\n");
    return 0;
}

/* ---------------------------------------------------------------- reduce */

static int cmd_reduce(const char *source, const char *state, const char *why,
                      const char *book_path, unsigned column)
{
    p10_reduction r;
    p10_status s = p10_reduce_file(source, column, &r);
    if (s != P10_OK) {
        fprintf(stderr, "p10: %s: %s\n", source, p10_strstatus(s));
        return EXIT_REFUSED;
    }

    char med[24];
    char p95[24];
    char bucket[24];
    if (!p10_format_current(med, sizeof med, r.median) ||
        !p10_format_current(p95, sizeof p95, r.p95) ||
        !p10_format_current(bucket, sizeof bucket, r.bucket))
        return EXIT_REFUSED;

    printf("%s: %lu samples, %lu refused\n", state, (unsigned long)r.n,
           (unsigned long)r.refused);
    printf("  median %s, p95 %s, both good to %s\n", med, p95, bucket);
    if (r.refused > 0u)
        printf("  the refused lines were not samples and were dropped, not\n"
               "  guessed at; a negative reading is one of the ways that\n"
               "  happens\n");

    if (book_path == NULL)
        return 0;

    p10_book b;
    if (p10_book_read(&b, book_path) != P10_OK)
        p10_book_init(&b);        /* a book that is not there yet is empty */

    s = p10_book_add(&b, state, &r, why);
    if (s != P10_OK) {
        fprintf(stderr, "p10: %s\n", p10_strstatus(s));
        return EXIT_REFUSED;
    }

    FILE *f = fopen(book_path, "wb");
    if (f == NULL) {
        fprintf(stderr, "p10: cannot write %s\n", book_path);
        return EXIT_REFUSED;
    }
    s = p10_book_write(&b, f);
    fclose(f);
    if (s != P10_OK) {
        fprintf(stderr, "p10: %s\n", p10_strstatus(s));
        return EXIT_REFUSED;
    }
    printf("  appended to %s, which now holds %u\n", book_path, b.n);
    return 0;
}

/* ----------------------------------------------------------------- scene */

static int cmd_scene(const char *kind_name, uint32_t samples, const char *out)
{
    p10_scene_kind kind;
    if (!p10_scene_of(kind_name, &kind)) {
        fprintf(stderr, "p10: --kind %s is not one of ramp, sleep, idle, "
                        "ble, usb\n", kind_name);
        return EXIT_USAGE;
    }

    FILE *f = stdout;
    if (out != NULL) {
        f = fopen(out, "wb");
        if (f == NULL) {
            fprintf(stderr, "p10: cannot write %s\n", out);
            return EXIT_REFUSED;
        }
    }
    p10_status s = p10_scene_write(kind, samples, f);
    if (out != NULL)
        fclose(f);
    if (s != P10_OK) {
        fprintf(stderr, "p10: %s\n", p10_strstatus(s));
        return EXIT_REFUSED;
    }
    if (out != NULL)
        printf("%s: %lu samples of %s\n", out, (unsigned long)samples,
               kind_name);
    return 0;
}

/* ------------------------------------------------------------------ args */

static bool number_of(const char *s, uint32_t lo, uint32_t hi, uint32_t *out)
{
    if (s == NULL || out == NULL)
        return false;
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 10);
    if (end == s || end == NULL || *end != '\0')
        return false;
    if (v < lo || v > hi)
        return false;
    *out = (uint32_t)v;
    return true;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fputs(USAGE, stderr);
        return EXIT_USAGE;
    }

    const char *cmd = argv[1];
    const char *kind = NULL;
    const char *source = NULL;
    const char *state = NULL;
    const char *why = NULL;
    const char *book = NULL;
    const char *out = NULL;
    uint32_t samples = 20000;
    uint32_t column = 0;

    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--kind") == 0 && i + 1 < argc) {
            kind = argv[++i];
        } else if (strcmp(a, "--source") == 0 && i + 1 < argc) {
            source = argv[++i];
        } else if (strcmp(a, "--state") == 0 && i + 1 < argc) {
            state = argv[++i];
        } else if (strcmp(a, "--why") == 0 && i + 1 < argc) {
            why = argv[++i];
        } else if (strcmp(a, "--book") == 0 && i + 1 < argc) {
            book = argv[++i];
        } else if (strcmp(a, "--out") == 0 && i + 1 < argc) {
            out = argv[++i];
        } else if (strcmp(a, "--samples") == 0 && i + 1 < argc) {
            if (!number_of(argv[++i], 1u, 10000000u, &samples)) {
                fprintf(stderr, "p10: --samples wants 1 to 10000000\n");
                return EXIT_USAGE;
            }
        } else if (strcmp(a, "--column") == 0 && i + 1 < argc) {
            if (!number_of(argv[++i], 0u, 63u, &column)) {
                fprintf(stderr, "p10: --column wants 0 to 63\n");
                return EXIT_USAGE;
            }
        } else {
            fprintf(stderr, "p10: unknown option %s\n\n", a);
            fputs(USAGE, stderr);
            return EXIT_USAGE;
        }
    }

    if (strcmp(cmd, "scene") == 0) {
        if (kind == NULL) {
            fprintf(stderr, "p10: scene wants --kind\n\n");
            fputs(USAGE, stderr);
            return EXIT_USAGE;
        }
        return cmd_scene(kind, samples, out);
    }
    if (strcmp(cmd, "reduce") == 0) {
        if (source == NULL || state == NULL) {
            fprintf(stderr, "p10: reduce wants --source and --state\n\n");
            fputs(USAGE, stderr);
            return EXIT_USAGE;
        }
        return cmd_reduce(source, state, why, book, column);
    }
    if (strcmp(cmd, "report") == 0 || strcmp(cmd, "judge") == 0) {
        if (book == NULL) {
            fprintf(stderr, "p10: %s wants --book\n\n", cmd);
            fputs(USAGE, stderr);
            return EXIT_USAGE;
        }
        return (cmd[0] == 'r') ? cmd_report(book) : cmd_judge(book);
    }

    fprintf(stderr, "p10: unknown command %s\n\n", cmd);
    fputs(USAGE, stderr);
    return EXIT_USAGE;
}
