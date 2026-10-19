/* P07 and P08's log checker, on the command line.
 *
 * `verify` is P07's fifth acceptance item and P08's first, and `pair` is
 * P08's third, which is the one that is about P07. That item is why these
 * two labs share a program.
 */
#define _POSIX_C_SOURCE 200809L

#include "p07.h"

#include <stdlib.h>
#include <string.h>

#define EXIT_USAGE   1
#define EXIT_REFUSED 2

static const char *USAGE =
    "usage: p07 <command> [options]\n"
    "\n"
    "  verify --source FILE --profile P\n"
    "          P is iks4a1 (P07), lowg or highg (P08). Exits 2 when the log\n"
    "          is not what its chapter says it is.\n"
    "  pair   --a FILE --b FILE\n"
    "          P08's third item, which is about P07: the numbers live in a\n"
    "          different log file. Exits 2 when they do not.\n"
    "  scene  --kind K [--records N] [--out FILE]\n"
    "          K is iks4a1, lowg, highg, drift, collapsed, hubbed, noscale.\n"
    "  profiles\n";

static int cmd_profiles(void)
{
    const p07_profile *p = p07_profiles();
    printf("Three profiles, two labs, and every key below is quoted from a\n"
           "chapter rather than chosen here.\n\n");
    for (unsigned i = 0; i < p07_profile_count(); i++) {
        printf("  %-7s %s\n", p[i].name, p[i].shield);
        printf("  %-7s the chapter calls the file %s\n", "", p[i].file);
        printf("  %-7s wants ", "");
        for (unsigned k = 0; k < p[i].required_n; k++)
            printf("%s%s", (k > 0u) ? ", " : "", p[i].required[k]);
        printf("\n  %-7s refuses ", "");
        for (unsigned k = 0; k < p[i].forbidden_n; k++)
            printf("%s%s", (k > 0u) ? ", " : "", p[i].forbidden[k]);
        if (p[i].pressure_needs_scale)
            printf("\n  %-7s and a pressure here needs its range beside it",
                   "");
        printf("\n\n");
    }
    printf("`acc` is refused everywhere because P08 names it: an analysis\n"
           "cannot find the two streams already merged in a field called acc\n"
           "and assume the merge was correct.\n");
    return 0;
}

static int cmd_verify(const char *source, const char *profile_name)
{
    const p07_profile *p = p07_profile_of(profile_name);
    if (p == NULL) {
        fprintf(stderr, "p07: --profile %s is not one of iks4a1, lowg, "
                        "highg\n", profile_name);
        return EXIT_USAGE;
    }

    static p07_log l;
    p07_status s = p07_read(source, p, &l);
    if (s != P07_OK) {
        fprintf(stderr, "p07: %s: %s\n", source, p07_strstatus(s));
        return EXIT_REFUSED;
    }

    p07_verdict v;
    p07_judge(&l, p, &v);

    printf("%s, against %s, which the chapter calls %s.\n",
           source, p->shield, p->file);
    printf("%lu lines, %lu records, %lu refused.\n",
           (unsigned long)l.lines, (unsigned long)l.records,
           (unsigned long)l.refused);
    if (l.records > 0u) {
        printf("The first record carries %u key%s:", l.first.n,
               (l.first.n == 1u) ? "" : "s");
        for (unsigned i = 0; i < l.first.n; i++)
            printf(" %s", l.first.k[i]);
        putchar('\n');
    }
    putchar('\n');

    if (!v.has_records)
        printf("REFUSED: not one line of this file is a JSON object, so\n"
               "there is no record here to check.\n");
    if (l.refused > 0u)
        printf("%lu line%s not a JSON object and %s counted and dropped\n"
               "rather than half read.\n", (unsigned long)l.refused,
               (l.refused == 1u) ? " was" : "s were",
               (l.refused == 1u) ? "was" : "were");

    if (!v.keys_stable)
        printf("REFUSED: the keys changed at line %lu. A schema that drifts\n"
               "half way through a capture leaves a file that no parser\n"
               "reads end to end. Restart the capture rather than patching\n"
               "the file.\n", (unsigned long)l.drift_line);

    if (!v.required_present)
        printf("REFUSED: %lu record%s missing %s, which this shield is\n"
               "supposed to report. A capture that is short from its first\n"
               "line is usually a sensor-hub or ISPU mode: those route parts\n"
               "through the LSM6, so the firmware scan finds fewer addresses\n"
               "than the shield carries and the record silently loses keys.\n",
               (unsigned long)l.missing_required,
               (l.missing_required == 1u) ? " is" : "s are", v.missing);

    if (!v.nothing_forbidden)
        printf("REFUSED: %lu record%s a key called %s.\n",
               (unsigned long)l.forbidden_seen,
               (l.forbidden_seen == 1u) ? " carries" : "s carry", v.present);
    if (!v.nothing_forbidden && strcmp(v.present, "acc") == 0)
        printf("\nThat is the one P08 names. Two streams, two keys, two log\n"
               "files: collapsing them into one acceleration field is how a\n"
               "wearable-grade number ends up wearing an industrial label.\n"
               "An analysis cannot find them already merged in a field\n"
               "called acc and assume the merge was correct.\n");

    if (!v.scale_with_pressure)
        printf("REFUSED: %lu record%s a pressure with no range beside it.\n"
               "The ILPS22QS has two scales, so a pressure without the one\n"
               "it was measured in is a number with two possible meanings.\n",
               (unsigned long)l.pressure_without_scale,
               (l.pressure_without_scale == 1u) ? " carries" : "s carry");

    if (l.any_overflow)
        printf("\nAt least one record had more keys, or a longer key, than\n"
               "this program holds. That is reported rather than trimmed,\n"
               "because a trimmed key set would compare equal to one that is\n"
               "not.\n");

    putchar('\n');
    printf("Not decided here: whether the numbers are right. Breathing on\n"
           "the humidity part and a palm over the temperature part are the\n"
           "items a person checks, and no amount of reading the file settles\n"
           "either.\n");

    if (!p07_verdict_passed(&v))
        return EXIT_REFUSED;

    printf("\nThis log is what its chapter says it is.\n");
    return 0;
}

static int cmd_pair(const char *a, const char *b)
{
    p07_pair r = p07_compare(a, b);
    printf("%s\n  and %s\n\n", a, b);
    printf("%s\n", p07_strpair(r));
    if (r == P07_PAIR_OK) {
        printf("\nWhich is P08's third acceptance item, and it is an item\n"
               "about P07: the numbers live in a different log file.\n");
        return 0;
    }
    printf("\nREFUSED. Two producers, two keys, two writers, two files. The\n"
           "chapter draws that as structure rather than as advice, because\n"
           "an analysis that wants both opens both and joins on time, which\n"
           "is an explicit operation somebody has to write down.\n");
    return EXIT_REFUSED;
}

static int cmd_scene(const char *kind_name, uint32_t records, const char *out)
{
    p07_scene_kind kind;
    if (!p07_scene_of(kind_name, &kind)) {
        fprintf(stderr, "p07: --kind %s is not one of iks4a1, lowg, highg, "
                        "drift, collapsed, hubbed, noscale\n", kind_name);
        return EXIT_USAGE;
    }

    FILE *f = stdout;
    if (out != NULL) {
        f = fopen(out, "wb");
        if (f == NULL) {
            fprintf(stderr, "p07: cannot write %s\n", out);
            return EXIT_REFUSED;
        }
    }
    p07_status s = p07_scene_write(kind, records, f);
    if (out != NULL)
        fclose(f);
    if (s != P07_OK) {
        fprintf(stderr, "p07: %s\n", p07_strstatus(s));
        return EXIT_REFUSED;
    }
    if (out != NULL)
        printf("%s: %lu records of %s, which no shield produced\n", out,
               (unsigned long)records, kind_name);
    return 0;
}

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
    const char *source = NULL;
    const char *profile = NULL;
    const char *kind = NULL;
    const char *out = NULL;
    const char *a = NULL;
    const char *b = NULL;
    uint32_t records = 200;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--source") == 0 && i + 1 < argc)
            source = argv[++i];
        else if (strcmp(argv[i], "--profile") == 0 && i + 1 < argc)
            profile = argv[++i];
        else if (strcmp(argv[i], "--kind") == 0 && i + 1 < argc)
            kind = argv[++i];
        else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc)
            out = argv[++i];
        else if (strcmp(argv[i], "--a") == 0 && i + 1 < argc)
            a = argv[++i];
        else if (strcmp(argv[i], "--b") == 0 && i + 1 < argc)
            b = argv[++i];
        else if (strcmp(argv[i], "--records") == 0 && i + 1 < argc) {
            if (!number_of(argv[++i], 1u, 1000000u, &records)) {
                fprintf(stderr, "p07: --records wants 1 to 1000000\n");
                return EXIT_USAGE;
            }
        } else {
            fprintf(stderr, "p07: unknown option %s\n\n", argv[i]);
            fputs(USAGE, stderr);
            return EXIT_USAGE;
        }
    }

    if (strcmp(cmd, "profiles") == 0)
        return cmd_profiles();
    if (strcmp(cmd, "verify") == 0) {
        if (source == NULL || profile == NULL) {
            fprintf(stderr, "p07: verify wants --source and --profile\n\n");
            fputs(USAGE, stderr);
            return EXIT_USAGE;
        }
        return cmd_verify(source, profile);
    }
    if (strcmp(cmd, "pair") == 0) {
        if (a == NULL || b == NULL) {
            fprintf(stderr, "p07: pair wants --a and --b\n\n");
            fputs(USAGE, stderr);
            return EXIT_USAGE;
        }
        return cmd_pair(a, b);
    }
    if (strcmp(cmd, "scene") == 0) {
        if (kind == NULL) {
            fprintf(stderr, "p07: scene wants --kind\n\n");
            fputs(USAGE, stderr);
            return EXIT_USAGE;
        }
        return cmd_scene(kind, records, out);
    }

    fprintf(stderr, "p07: unknown command %s\n\n", cmd);
    fputs(USAGE, stderr);
    return EXIT_USAGE;
}
