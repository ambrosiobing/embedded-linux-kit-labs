/* P06, the host half of the 8x8 time-of-flight occupancy kiosk.
 *
 * Sixty-four distances arrive on one line and sixty-four rectangles are
 * painted. Between those two there is one comparison per zone against a
 * threshold measured with the field of view empty, and the whole of this
 * program is about making that comparison honest.
 *
 * The rules are the ones the other two labs in this volume are written
 * against, with one of their own.
 *
 * No allocation on the data path. Every buffer is a fixed array sized at
 * compile time, the frame included.
 *
 * Library code returns a status and never exits. Only main.c decides the
 * program stops.
 *
 * Nothing is repaired. A line that is not exactly the contract is dropped and
 * counted, never patched up to the right length. The chapter is explicit
 * about why: a partial frame painted on the glass looks like a real reading
 * and is not.
 *
 * And the one of its own: nothing is painted that was not parsed. The paint
 * happens in the same path as the parse, so a sensor that stops sending stops
 * the repaint, rather than leaving a frame on the glass that is a minute old
 * and looks live.
 *
 * Two numbers in here are conventions rather than measurements, and are
 * written down as such. Zone 0 is painted at the top left and the zones run
 * row by row, which is a choice this program fixes and the bench has to
 * confirm by putting a hand in a known corner. And a zone reporting zero
 * millimetres is the sensor saying it found no target there, not a target at
 * zero distance, so a zero is never the nearest thing in the room.
 */
#ifndef P06_H
#define P06_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../../lib/framebuffer.h"
#include "../../lib/linereader.h"

/* ---------------------------------------------------------------- limits */

#define P06_SIDE 8
#define P06_ZONES (P06_SIDE * P06_SIDE)
/* The literal Z, then one distance per zone. */
#define P06_FIELDS (P06_ZONES + 1)

/* The VL53L8CX does not range past about four metres. A larger number on the
 * line is a line this program does not understand, and it is dropped rather
 * than painted as a very distant wall. */
#define P06_RANGE_MAX_MM 4000

/* A baseline taken from a handful of frames is not a baseline. */
#define P06_BASELINE_MIN_FRAMES 10

/* The acceptance test asks that an empty field of view report the wall within
 * plus or minus 20 mm. A baseline whose own frames disagree by more than that
 * is refused, because the number it would produce is not a measurement of a
 * wall, it is a measurement of something moving. */
#define P06_STABILITY_LIMIT_MM 20

/* How far under the nearest wall the threshold sits by default. */
#define P06_MARGIN_MM 150

typedef enum {
    P06_OK = 0,
    P06_E_IO,
    P06_E_FORMAT,
    P06_E_RANGE,
    P06_E_ARG,
    P06_E_EOF
} p06_status;

const char *p06_strerror(p06_status s);

/* ---------------------------------------------------------------- frames */

typedef struct {
    uint16_t mm[P06_ZONES];
} p06_frame;

/* Why a line was not a frame. Counted separately because they have different
 * causes: a line that is not ours, a truncated line, rubbish in a field, and
 * a number no sensor in this kit can produce. */
typedef enum {
    P06_DROP_NONE = 0,
    P06_DROP_NOT_Z,
    P06_DROP_FIELD_COUNT,
    P06_DROP_NOT_INTEGER,
    P06_DROP_OUT_OF_RANGE
} p06_drop;

/* Exactly P06_FIELDS comma-separated fields, the first the literal Z, the
 * rest decimal millimetres. Anything else fills *why and returns an error. */
p06_status p06_parse_frame(const char *line, p06_frame *f, p06_drop *why);

/* The nearest real target, in millimetres, and how many zones saw one. Zeroes
 * are not targets and take no part in either answer. Returns 0 when no zone
 * saw anything. */
uint16_t p06_frame_near_mm(const p06_frame *f, uint16_t *zones_with_target);
uint16_t p06_frame_far_mm(const p06_frame *f);

/* ---------------------------------------------------------------- the book */

/* What the bench measured with the field of view empty, and the threshold
 * that follows from it. Never a constant: the wall is a property of where the
 * sensor sits, so this file belongs to one placement and says which. */
typedef struct {
    uint16_t wall_mm[P06_ZONES];    /* the mean over the baseline frames */
    uint16_t spread_mm[P06_ZONES];  /* the largest disagreement, per zone */
    uint16_t nearest_wall_mm;
    uint16_t threshold_mm;          /* occupied is a target nearer than this */
    uint16_t margin_mm;
    uint16_t worst_spread_mm;
    uint32_t frames;
    char placement[128];
} p06_book;

void p06_book_init(p06_book *b);

/* Accumulate one frame of an empty field of view. A frame with any zone
 * reporting no target is refused: a baseline is taken against a surface the
 * sensor can see, and a hole in it would leave a zone with no wall. */
p06_status p06_book_add(p06_book *b, const p06_frame *f);
/* Finish the measurement and derive the threshold. Refuses a baseline with
 * too few frames, and one whose own frames disagree by more than the
 * stability limit. */
p06_status p06_book_finish(p06_book *b, uint16_t margin_mm, uint16_t stability_limit_mm);
p06_status p06_book_write(const p06_book *b, const char *path);
p06_status p06_book_read(p06_book *b, const char *path);

/* ---------------------------------------------------------------- occupancy */

typedef struct {
    bool occupied[P06_ZONES];
    uint16_t count;
    uint16_t near_mm;
} p06_occupancy;

p06_status p06_classify(const p06_frame *f, const p06_book *b, p06_occupancy *o);

/* The 8x8 grid as text, for the step of the chapter that says to confirm the
 * numbers before painting anything. Needs no display of any kind. */
void p06_frame_print(const p06_frame *f, const p06_book *b, FILE *out);

/* ---------------------------------------------------------------- the glass */

typedef fb_geometry p06_geometry;

/* From the two text files the kernel already publishes, which saves an ioctl
 * and a header, and lets a test point this at a directory of its own. */
p06_status p06_geometry_from_sysfs(p06_geometry *g, const char *dir);
/* From a string of the form 480x320x16, for a bench with neither. */
p06_status p06_geometry_parse(p06_geometry *g, const char *spec);

typedef fb_target p06_fb;

void p06_fb_init(p06_fb *fb);
/* Maps exactly width * height * bpp / 8 bytes and refuses to open a regular
 * file shorter than that. Nothing in here ever writes past that length. */
p06_status p06_fb_open(p06_fb *fb, const char *path, const p06_geometry *g);
p06_status p06_fb_close(p06_fb *fb);
/* Clipped to the geometry. A rectangle that starts outside it paints
 * nothing and is not an error, because the grid arithmetic is allowed to be
 * simple only if this is not. */
p06_status p06_fb_fill(p06_fb *fb, uint32_t x, uint32_t y,
                       uint32_t w, uint32_t h, uint32_t rgb);
p06_status p06_fb_paint(p06_fb *fb, const p06_frame *f, const p06_book *b);

/* The colour a zone is painted, as 0xRRGGBB, before it is packed for the
 * display. Exposed so that the suite can assert a colour rather than a pixel
 * value it worked out for itself. */
uint32_t p06_zone_colour(uint16_t mm, const p06_book *b);

/* ---------------------------------------------------------------- the run */

typedef struct {
    uint32_t accepted;
    uint32_t painted;
    uint32_t dropped[5];      /* indexed by p06_drop */
    uint32_t long_lines;
    uint32_t blank_lines;
    uint16_t last_occupied;
    uint16_t min_occupied;
    uint16_t max_occupied;
    uint16_t near_min_mm;     /* across the run */
    uint16_t near_max_mm;
    bool started;
} p06_run;

void p06_run_init(p06_run *run);
uint32_t p06_run_dropped_total(const p06_run *run);

/* Read source, classify every frame against the book, and paint each one if
 * fb is not NULL. max_frames of 0 means to the end of the stream. The book
 * may be NULL, in which case frames are parsed and reported and nothing is
 * classified, which is the state of the lab before the wall is measured. */
p06_status p06_run_stream(const char *source, const p06_book *b, p06_fb *fb,
                          uint32_t max_frames, p06_run *run);

/* ---------------------------------------------------------------- synthesis */

typedef enum {
    P06_SCENE_EMPTY = 0,   /* the wall, with a little noise */
    P06_SCENE_HAND,        /* a hand at 300 mm over a patch of zones */
    P06_SCENE_WALK         /* the hand's patch moves across the field */
} p06_scene;

typedef enum {
    P06_FAULT_SHORT = 0,   /* one field missing */
    P06_FAULT_NOT_Z,       /* the line does not begin with Z */
    P06_FAULT_RUBBISH,     /* a field that is not a number */
    P06_FAULT_HUGE,        /* a distance no sensor here can report */
    P06_FAULT_LONG         /* a line longer than any reader's buffer */
} p06_fault_kind;

#define P06_FAULTS_MAX 32

typedef struct {
    uint32_t at;           /* the frame index the fault applies to */
    p06_fault_kind kind;
} p06_fault;

typedef struct {
    uint32_t frames;
    p06_scene scene;
    uint16_t wall_mm;
    uint16_t hand_mm;
    uint16_t hand_zones;   /* how many zones the hand covers */
    uint16_t noise_mm;     /* plus or minus, deterministic from the seed */
    uint32_t seed;
    p06_fault faults[P06_FAULTS_MAX];
    uint16_t fault_count;
} p06_synth;

void p06_synth_init(p06_synth *s);
p06_status p06_synth_add_fault(p06_synth *s, p06_fault_kind kind, uint32_t at);
p06_status p06_synth_write(const p06_synth *s, const char *path);

#endif /* P06_H */
