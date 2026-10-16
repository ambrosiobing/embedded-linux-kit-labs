/* P05 on the command line.
 *
 * Exit status. Zero means the question was answered. Two means the command
 * line was wrong. One means either the question could not be answered, with
 * the reason on standard error, or the acceptance test was applied and not
 * met.
 *
 * verify has a third outcome, and it is the reason this program exists.
 * Every item this bench can settle may pass while one item remains
 * unsettlable here, and that is neither a pass nor an ordinary failure. It
 * exits 3, and says in words that a multimeter is what moves it.
 */

#include "p05.h"

#include <stdlib.h>
#include <string.h>

static void usage(FILE *out)
{
    fprintf(out,
        "usage: p05 <command> [options]\n"
        "\n"
        "  scene   --kind good|hole|repeat|short|rail-low|over-range\n"
        "          --out FILE [--rows N]\n"
        "  check   --source FILE\n"
        "  book    --out FILE [--rail-3v3 V] [--rail-5v V] [--instrument TEXT]\n"
        "          [--address N] [--header-clear] [--survived-reboot]\n"
        "          [--second-run-clean] [--date TEXT]\n"
        "          [--os TEXT] [--kernel TEXT] [--model TEXT] [--root TEXT]\n"
        "  verify  --source FILE --book FILE\n"
        "\n"
        "The recorder is not here. The acquisition is four calls into the\n"
        "vendor's library, which owns the calibration in the board's EEPROM,\n"
        "and the chapter prints them. This program starts at the file.\n");
}

typedef struct {
    const char *source;
    const char *out;
    const char *book;
    const char *kind;
    const char *instrument;
    const char *date;
    const char *os;
    const char *kernel;
    const char *model;
    const char *root;
    long rows;
    long address;
    double rail_3v3;
    double rail_5v;
    bool have_3v3;
    bool have_5v;
    bool have_address;
    bool header_clear;
    bool survived_reboot;
    bool second_run_clean;
} opts;

static void opts_init(opts *o)
{
    memset(o, 0, sizeof *o);
    o->rows = P05_EXPECT_ROWS;
    o->instrument = "none";
    o->date = "not stated";
}

#define NEED_VALUE()                                                        \
    do {                                                                    \
        if (i + 1 >= argc) {                                                \
            fprintf(stderr, "p05: %s needs a value\n", a);                  \
            return false;                                                   \
        }                                                                   \
        v = argv[++i];                                                      \
    } while (0)

static bool parse_args(opts *o, int argc, char **argv)
{
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        const char *v = NULL;
        if (strcmp(a, "--source") == 0)            { NEED_VALUE(); o->source = v; }
        else if (strcmp(a, "--out") == 0)          { NEED_VALUE(); o->out = v; }
        else if (strcmp(a, "--book") == 0)         { NEED_VALUE(); o->book = v; }
        else if (strcmp(a, "--kind") == 0)         { NEED_VALUE(); o->kind = v; }
        else if (strcmp(a, "--instrument") == 0)   { NEED_VALUE(); o->instrument = v; }
        else if (strcmp(a, "--date") == 0)         { NEED_VALUE(); o->date = v; }
        else if (strcmp(a, "--os") == 0)           { NEED_VALUE(); o->os = v; }
        else if (strcmp(a, "--kernel") == 0)       { NEED_VALUE(); o->kernel = v; }
        else if (strcmp(a, "--model") == 0)        { NEED_VALUE(); o->model = v; }
        else if (strcmp(a, "--root") == 0)         { NEED_VALUE(); o->root = v; }
        else if (strcmp(a, "--rows") == 0)         { NEED_VALUE(); o->rows = atol(v); }
        else if (strcmp(a, "--address") == 0)      { NEED_VALUE(); o->address = atol(v);
                                                     o->have_address = true; }
        else if (strcmp(a, "--rail-3v3") == 0)     { NEED_VALUE(); o->rail_3v3 = atof(v);
                                                     o->have_3v3 = true; }
        else if (strcmp(a, "--rail-5v") == 0)      { NEED_VALUE(); o->rail_5v = atof(v);
                                                     o->have_5v = true; }
        else if (strcmp(a, "--header-clear") == 0)      o->header_clear = true;
        else if (strcmp(a, "--survived-reboot") == 0)   o->survived_reboot = true;
        else if (strcmp(a, "--second-run-clean") == 0)  o->second_run_clean = true;
        else {
            fprintf(stderr, "p05: unknown option %s\n", a);
            return false;
        }
    }
    return true;
}

static int cmd_scene(const opts *o)
{
    if (o->out == NULL || o->kind == NULL) {
        fprintf(stderr, "p05: --kind and --out are both required\n");
        return 2;
    }
    p05_scene scene;
    if (strcmp(o->kind, "good") == 0)            scene = P05_SCENE_GOOD;
    else if (strcmp(o->kind, "hole") == 0)       scene = P05_SCENE_HOLE;
    else if (strcmp(o->kind, "repeat") == 0)     scene = P05_SCENE_REPEAT;
    else if (strcmp(o->kind, "short") == 0)      scene = P05_SCENE_SHORT;
    else if (strcmp(o->kind, "rail-low") == 0)   scene = P05_SCENE_RAIL_LOW;
    else if (strcmp(o->kind, "over-range") == 0) scene = P05_SCENE_OVER_RANGE;
    else {
        fprintf(stderr, "p05: --kind must be good, hole, repeat, short, "
                        "rail-low or over-range\n");
        return 2;
    }
    if (o->rows <= 0) {
        fprintf(stderr, "p05: --rows must be positive\n");
        return 2;
    }

    p05_status st = p05_scene_write(scene, (uint32_t)o->rows, o->out);
    if (st != P05_OK) {
        fprintf(stderr, "p05: %s: %s\n", o->out, p05_strerror(st));
        return 1;
    }
    printf("wrote the %s file, %ld rows asked for, to %s\n",
           o->kind, o->rows, o->out);
    return 0;
}

static int cmd_check(const opts *o)
{
    if (o->source == NULL) {
        fprintf(stderr, "p05: --source is required\n");
        return 2;
    }
    p05_scan s;
    p05_status st = p05_scan_read(o->source, &s);
    if (st != P05_OK) {
        fprintf(stderr, "p05: %s: %s\n", o->source, p05_strerror(st));
        return 1;
    }
    p05_scan_print(&s, stdout);
    if (!p05_scan_whole(&s)) {
        printf("\nThis file is not whole. A plot of it would look like a "
               "recording.\n");
        return 1;
    }
    printf("\nThe file is whole: no holes, no repeats, nothing out of order, "
           "and it starts at zero.\n");
    return 0;
}

static int cmd_book(const opts *o)
{
    if (o->out == NULL) {
        fprintf(stderr, "p05: --out is required\n");
        return 2;
    }
    p05_book b;
    p05_book_init(&b);

    snprintf(b.instrument, sizeof b.instrument, "%s", o->instrument);
    snprintf(b.date, sizeof b.date, "%s", o->date);
    if (o->os != NULL)     snprintf(b.os_release, sizeof b.os_release, "%s", o->os);
    if (o->kernel != NULL) snprintf(b.kernel, sizeof b.kernel, "%s", o->kernel);
    if (o->model != NULL)  snprintf(b.model, sizeof b.model, "%s", o->model);
    if (o->root != NULL)   snprintf(b.root_fs, sizeof b.root_fs, "%s", o->root);

    b.have_rail_3v3 = o->have_3v3;
    b.rail_3v3_v = o->rail_3v3;
    b.have_rail_5v = o->have_5v;
    b.rail_5v_v = o->rail_5v;
    b.have_board = o->have_address;
    b.board_address = (uint8_t)o->address;
    b.have_header_clear = o->header_clear;
    b.header_clear = o->header_clear;
    b.have_survived_reboot = o->survived_reboot;
    b.survived_reboot = o->survived_reboot;
    b.have_second_run = o->second_run_clean;
    b.second_run_clean = o->second_run_clean;

    p05_status st = p05_book_write(&b, o->out);
    if (st != P05_OK) {
        fprintf(stderr, "p05: %s: %s\n", o->out, p05_strerror(st));
        return 1;
    }
    printf("wrote the lab book to %s\n", o->out);
    if (!b.have_rail_3v3)
        printf("with no independent reading of the 3.3 V rail in it, which is "
               "the item this bench cannot settle\n");
    return 0;
}

static int cmd_verify(const opts *o)
{
    if (o->source == NULL || o->book == NULL) {
        fprintf(stderr, "p05: --source and --book are both required\n");
        return 2;
    }

    p05_scan s;
    p05_status st = p05_scan_read(o->source, &s);
    if (st != P05_OK) {
        fprintf(stderr, "p05: %s: %s\n", o->source, p05_strerror(st));
        return 1;
    }
    p05_book b;
    if (p05_book_read(&b, o->book) != P05_OK) {
        fprintf(stderr, "p05: %s cannot be read as a lab book\n", o->book);
        return 1;
    }

    p05_verdict v;
    if (p05_judge(&s, &b, &v) != P05_OK) {
        fprintf(stderr, "p05: %s has no rows to judge, or fewer than two "
                        "channels\n", o->source);
        return 1;
    }

    p05_verdict_print(&v, stdout);
    if (p05_verdict_passed(&v))
        return 0;
    if (p05_verdict_passed_what_is_possible(&v))
        return 3;
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage(stderr);
        return 2;
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage(stdout);
        return 0;
    }

    opts o;
    opts_init(&o);
    if (!parse_args(&o, argc, argv))
        return 2;

    const char *cmd = argv[1];
    if (strcmp(cmd, "scene") == 0)  return cmd_scene(&o);
    if (strcmp(cmd, "check") == 0)  return cmd_check(&o);
    if (strcmp(cmd, "book") == 0)   return cmd_book(&o);
    if (strcmp(cmd, "verify") == 0) return cmd_verify(&o);

    fprintf(stderr, "p05: unknown command %s\n", cmd);
    usage(stderr);
    return 2;
}
