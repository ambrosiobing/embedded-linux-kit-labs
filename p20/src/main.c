/* P20's fleet judge, on the command line.
 *
 * Two commands and no SSH. `scene` writes a fleet file that no sweep
 * produced, and `judge` reads one and decides whether it is the acceptance
 * artifact the chapter says it is. There is deliberately no command that
 * runs the sweep: that is the chapter's forty lines of shell, it is the
 * right answer, and a C program that reimplemented it would be a worse one.
 */
#define _POSIX_C_SOURCE 200809L

#include "p20.h"

#include <string.h>

#define EXIT_USAGE   1
#define EXIT_REFUSED 2

static const char *USAGE =
    "usage: p20 <command> [options]\n"
    "\n"
    "  judge --source FILE     read a fleet file and decide it; exits 2 when\n"
    "                          the file is not an acceptance artifact\n"
    "  scene --kind K [--out FILE]\n"
    "                          K is good, blank-hat, fifth, two-hats,\n"
    "                          undated or pi-no-temp. Each of the five is\n"
    "                          wrong in exactly one way.\n"
    "\n"
    "There is no command here that runs the sweep. That is the chapter's\n"
    "shell script and it is the right answer; this reads what it wrote.\n";

static int cmd_judge(const char *path)
{
    p20_sweep s;
    p20_status st = p20_read(path, &s);
    if (st != P20_OK) {
        fprintf(stderr, "p20: %s: %s\n", path, p20_strstatus(st));
        return EXIT_REFUSED;
    }

    p20_verdict v;
    p20_judge(&s, &v);
    const p20_host *fleet = p20_fleet();

    printf("%lu lines, %u of the four hosts present.\n\n",
           (unsigned long)s.lines, v.present);
    for (unsigned i = 0; i < P20_HOSTS; i++) {
        const p20_block *b = &s.b[i];
        printf("  %-6s %-18s ", fleet[i].name, fleet[i].what);
        if (!b->seen) {
            printf("did not answer\n");
            continue;
        }
        if (!b->hat_reported)
            printf("HAT not reported");
        else if (b->hats_named > 1u)
            /* Only the last name is kept, so say that rather than printing
             * one of two as though it were the one that is seated. */
            printf("%u HATs named, the last of them %s", b->hats_named,
                   b->hat);
        else if (b->hat_named)
            printf("HAT %s", b->hat);
        else
            printf("header empty, reported");
        putchar('\n');
    }

    putchar('\n');
    if (!v.four_hosts)
        printf("REFUSED: the fleet is four hosts and %u answered.\n",
               v.present);
    if (!v.no_stranger)
        printf("REFUSED: %s is in this file and is not in the fleet. The\n"
               "fleet is four hosts, and a fifth name is a host this book\n"
               "has not brought up.\n", s.stranger);
    if (!v.every_hat_reported)
        printf("REFUSED: %s reported no HAT at all. An empty header is\n"
               "reported as none; a missing line reads as a broken script\n"
               "rather than as an empty header, and the two must not look\n"
               "the same in the artifact.\n",
               fleet[v.first_silent_hat].name);
    if (!v.exclusive)
        printf("REFUSED: %s names more than one 40-pin HAT. One board owns\n"
               "the header, which is the rule every lab in this volume is\n"
               "written against.\n", fleet[v.doubled].name);
    if (!v.dated)
        printf("REFUSED: this file carries no timestamp, so it cannot be\n"
               "compared with a bench a person can see. A sweep taken in\n"
               "the middle of a teardown describes a bench that no longer\n"
               "exists, and without a date there is no way to tell.\n");
    if (v.gaps > 0u)
        printf("REFUSED: %u field%s missing where the host was expected to\n"
               "answer, the first on %s. That is a command that failed, not\n"
               "a command that is absent.\n",
               v.gaps, (v.gaps == 1u) ? " is" : "s are",
               fleet[v.first_gap_host].name);

    if (v.honest_absences > 0u) {
        putchar('\n');
        printf("%u absence%s honest and %s not counted against this sweep.\n",
               v.honest_absences, (v.honest_absences == 1u) ? " is" : "s are",
               (v.honest_absences == 1u) ? "is" : "are");
        printf("vcgencmd is a Raspberry Pi tool and the NanoPi has none, and\n"
               "its header bus is 0 rather than 1, which P09 established. A\n"
               "shorter block for that host is a true statement about it.\n");
    }

    putchar('\n');
    printf("Not decided here: whether this file matches the bench. Four\n"
           "names and a reported HAT each is what a program can see; whether\n"
           "the board it names is the board on the table is the question the\n"
           "chapter asks a person to answer.\n");

    if (!p20_verdict_passed(&v))
        return EXIT_REFUSED;

    printf("\nThis file is an acceptance artifact.\n");
    return 0;
}

static int cmd_scene(const char *kind_name, const char *out)
{
    p20_scene_kind kind;
    if (!p20_scene_of(kind_name, &kind)) {
        fprintf(stderr, "p20: --kind %s is not one of good, blank-hat, "
                        "fifth, two-hats, undated, pi-no-temp\n", kind_name);
        return EXIT_USAGE;
    }

    FILE *f = stdout;
    if (out != NULL) {
        f = fopen(out, "wb");
        if (f == NULL) {
            fprintf(stderr, "p20: cannot write %s\n", out);
            return EXIT_REFUSED;
        }
    }
    p20_status s = p20_scene_write(kind, f);
    if (out != NULL)
        fclose(f);
    if (s != P20_OK) {
        fprintf(stderr, "p20: %s\n", p20_strstatus(s));
        return EXIT_REFUSED;
    }
    if (out != NULL)
        printf("%s: a sweep that did not happen, %s\n", out, kind_name);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fputs(USAGE, stderr);
        return EXIT_USAGE;
    }

    const char *cmd = argv[1];
    const char *source = NULL;
    const char *kind = NULL;
    const char *out = NULL;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--source") == 0 && i + 1 < argc)
            source = argv[++i];
        else if (strcmp(argv[i], "--kind") == 0 && i + 1 < argc)
            kind = argv[++i];
        else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc)
            out = argv[++i];
        else {
            fprintf(stderr, "p20: unknown option %s\n\n", argv[i]);
            fputs(USAGE, stderr);
            return EXIT_USAGE;
        }
    }

    if (strcmp(cmd, "judge") == 0) {
        if (source == NULL) {
            fprintf(stderr, "p20: judge wants --source\n\n");
            fputs(USAGE, stderr);
            return EXIT_USAGE;
        }
        return cmd_judge(source);
    }
    if (strcmp(cmd, "scene") == 0) {
        if (kind == NULL) {
            fprintf(stderr, "p20: scene wants --kind\n\n");
            fputs(USAGE, stderr);
            return EXIT_USAGE;
        }
        return cmd_scene(kind, out);
    }

    fprintf(stderr, "p20: unknown command %s\n\n", cmd);
    fputs(USAGE, stderr);
    return EXIT_USAGE;
}
