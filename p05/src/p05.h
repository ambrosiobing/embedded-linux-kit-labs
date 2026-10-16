/* P05, the file half of the calibrated voltage recorder.
 *
 * This lab is the only one in the volume that cannot reach a pass on this
 * bench, and the program exists to say so precisely rather than in a
 * paragraph somebody has to read.
 *
 * Its acceptance test has eight items. Four are measured from the file the
 * board produced. Three are facts the operator observed and wrote down, and
 * the most this program can do with those is notice that they were not
 * written down. One of them cannot be satisfied here at all: the multimeter
 * reading of the 3.3 V rail and the CH0 column of the capture have to agree,
 * and there is no voltmeter on this bench. The MCC 118 cannot stand in for
 * it, because the whole point of that item is an independent instrument
 * disagreeing with the one under test.
 *
 * So the verdict distinguishes three things that are usually run together:
 * an item that passed, an item that was never recorded, and an item that
 * cannot be recorded here with the instruments in the room. Only the last
 * of those is unfixable by spending an afternoon on it, and a report that
 * blurred them would make the cheapest useful purchase on this bench
 * invisible.
 *
 * What this program is not: the recorder. The acquisition is four calls
 * into the vendor's own library, that library owns the calibration held in
 * the board's EEPROM, and nothing here reimplements or wraps it. The
 * chapter prints those four calls, from the vendor's C examples, and this
 * program starts where their file does.
 *
 * The rules of the volume hold: nothing allocates on the data path, library
 * code returns a status and never exits, and nothing is repaired. A row
 * that does not parse is counted, never interpolated.
 */
#ifndef P05_H
#define P05_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../../lib/linereader.h"

#define P05_CHANNELS_MAX 8
#define P05_TEXT_MAX 128

/* The chapter's own numbers. */
#define P05_RAIL_3V3 3.30
#define P05_RAIL_5V 5.00
#define P05_RAIL_TOLERANCE 0.05
#define P05_CH1_TOLERANCE 0.25
#define P05_EXPECT_ROWS 10000
/* Never exceed plus or minus 10.1 V on an input. A sample beyond that is
 * either a wiring fault or a file this program should not be reading. */
#define P05_INPUT_LIMIT_V 10.1

typedef enum {
    P05_OK = 0,
    P05_E_IO,
    P05_E_FORMAT,
    P05_E_ARG
} p05_status;

const char *p05_strerror(p05_status s);

/* ---------------------------------------------------------------- the file */

typedef struct {
    uint32_t rows;              /* rows that parsed */
    uint32_t malformed;         /* rows that did not, counted and dropped */
    uint32_t channels;
    uint32_t holes;             /* indices that are missing between two rows */
    uint32_t repeats;           /* an index seen twice in a row */
    uint32_t backwards;         /* an index smaller than the one before it */
    int64_t first_index;
    int64_t last_index;
    bool starts_at_zero;
    double mean[P05_CHANNELS_MAX];
    double min[P05_CHANNELS_MAX];
    double max[P05_CHANNELS_MAX];
    uint32_t out_of_range;      /* samples beyond the input limit */
    bool header_seen;
} p05_scan;

void p05_scan_init(p05_scan *s);
p05_status p05_scan_read(const char *path, p05_scan *s);
void p05_scan_print(const p05_scan *s, FILE *out);
bool p05_scan_whole(const p05_scan *s);

/* ---------------------------------------------------------------- the book */

/* What the operator saw, which is the half of the acceptance test no file
 * can carry. Every field has an absent state, and absent is never read as
 * false: a reading nobody took and a reading that failed are different
 * things and the report keeps them apart. */
typedef struct {
    bool have_rail_3v3;
    double rail_3v3_v;
    bool have_rail_5v;
    double rail_5v_v;
    char instrument[P05_TEXT_MAX];   /* what measured the rails, if anything */

    bool have_board;
    uint8_t board_address;
    bool header_clear;               /* nothing else on the 40-pin header */
    bool have_header_clear;
    bool survived_reboot;
    bool have_survived_reboot;
    bool second_run_clean;           /* stop and cleanup left the board usable */
    bool have_second_run;

    char os_release[P05_TEXT_MAX];
    char kernel[P05_TEXT_MAX];
    char model[P05_TEXT_MAX];
    char root_fs[P05_TEXT_MAX];
    char date[P05_TEXT_MAX];
} p05_book;

void p05_book_init(p05_book *b);
p05_status p05_book_write(const p05_book *b, const char *path);
p05_status p05_book_read(p05_book *b, const char *path);

/* ---------------------------------------------------------------- the gate */

typedef enum {
    P05_ITEM_PASS = 0,
    P05_ITEM_FAIL,
    P05_ITEM_UNRECORDED,     /* nobody wrote it down, and an afternoon fixes that */
    P05_ITEM_IMPOSSIBLE      /* the instrument to settle it is not on this bench */
} p05_item;

typedef struct {
    p05_item ch0_on_rail;
    p05_item sample_count;
    p05_item board_named;
    p05_item index_monotonic;
    p05_item header_clear;
    p05_item ch1_near_5v;
    p05_item instrument_agrees;    /* the one a multimeter buys */
    p05_item second_run_clean;
    double ch0_mean;
    double ch1_mean;
    double instrument_difference;  /* only when both readings exist */
} p05_verdict;

p05_status p05_judge(const p05_scan *s, const p05_book *b, p05_verdict *v);
void p05_verdict_print(const p05_verdict *v, FILE *out);
/* Every item passed. An unrecorded or impossible item is not a pass. */
bool p05_verdict_passed(const p05_verdict *v);
/* Every item that this bench can settle, passed. The distinction is the
 * whole reason this lab has a program. */
bool p05_verdict_passed_what_is_possible(const p05_verdict *v);

/* ---------------------------------------------------------------- synthesis */

typedef enum {
    P05_SCENE_GOOD = 0,
    P05_SCENE_HOLE,          /* a block of indices missing from the middle */
    P05_SCENE_REPEAT,        /* one index written twice */
    P05_SCENE_SHORT,         /* fewer rows than a second of recording */
    P05_SCENE_RAIL_LOW,      /* CH0 sitting off the rail */
    P05_SCENE_OVER_RANGE     /* a sample past the input limit */
} p05_scene;

p05_status p05_scene_write(p05_scene scene, uint32_t rows, const char *path);

#endif /* P05_H */
