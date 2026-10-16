/* The suite. No framework, no hardware, no display, no sensor.
 *
 * Six groups, kept apart because they fail for different reasons:
 *
 *   parser      whether a line that is not the contract is dropped, and
 *               whether the reason given is the right one of four
 *   frame       whether a zone with no target is kept out of the answer
 *   book        whether a threshold can be produced from a measurement that
 *               does not deserve the name, which it must not be
 *   glass       whether anything is ever written outside the mapping, and
 *               whether the geometry is read rather than assumed
 *   synthesis   whether the same request twice gives the same stream
 *   the claim   whether anything is painted that was not parsed, which is
 *               this lab's own rule and the only one worth publishing
 *
 * The glass group paints into a file rather than a display, through the same
 * mmap path a framebuffer takes, with a guard area past the end of the
 * geometry that must come back untouched.
 */
#define _POSIX_C_SOURCE 200809L

#include "../src/p06.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

/* ---------------------------------------------------------------- helpers */

static const char *TMP_STREAM = "test_p06.stream";
static const char *TMP_OTHER  = "test_p06_other.stream";
static const char *TMP_BOOK   = "test_p06.book";
static const char *TMP_GLASS  = "test_p06.glass";
static const char *TMP_SYSFS  = "test_p06_sysfs";

#define GUARD_BYTES 256
#define GUARD_FILL 0xAA

static bool make_line(char *out, size_t cap, const uint16_t *mm, uint16_t n)
{
    size_t used = 0;
    int w = snprintf(out, cap, "Z");
    if (w < 0)
        return false;
    used = (size_t)w;
    for (uint16_t i = 0; i < n; i++) {
        w = snprintf(out + used, cap - used, ",%u", (unsigned)mm[i]);
        if (w < 0 || (size_t)w >= cap - used)
            return false;
        used += (size_t)w;
    }
    return true;
}

/* Two spare entries, because one of the parser's cases is a line with more
 * fields than the contract and the array has to hold what it asks for. */
#define FLAT_MAX (P06_ZONES + 2)

static bool flat_line(char *out, size_t cap, uint16_t value, uint16_t n)
{
    static uint16_t mm[FLAT_MAX];
    if (n > FLAT_MAX)
        return false;
    for (uint16_t i = 0; i < FLAT_MAX; i++)
        mm[i] = value;
    return make_line(out, cap, mm, n);
}

static bool make_glass(const char *path, size_t bytes, size_t guard)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return false;
    for (size_t i = 0; i < bytes + guard; i++)
        if (fputc(GUARD_FILL, fp) == EOF) {
            fclose(fp);
            return false;
        }
    return fclose(fp) == 0;
}

static bool guard_intact(const char *path, size_t bytes, size_t guard)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return false;
    if (fseek(fp, (long)bytes, SEEK_SET) != 0) {
        fclose(fp);
        return false;
    }
    bool ok_so_far = true;
    for (size_t i = 0; i < guard; i++) {
        int c = fgetc(fp);
        if (c != GUARD_FILL) {
            ok_so_far = false;
            break;
        }
    }
    fclose(fp);
    return ok_so_far;
}

static bool write_text(const char *path, const char *text)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return false;
    size_t n = strlen(text);
    size_t w = fwrite(text, 1, n, fp);
    return fclose(fp) == 0 && w == n;
}

static bool write_sysfs(const char *dir, const char *size, const char *depth)
{
    mkdir(dir, 0755);
    char path[512];
    snprintf(path, sizeof path, "%s/virtual_size", dir);
    if (!write_text(path, size))
        return false;
    snprintf(path, sizeof path, "%s/bits_per_pixel", dir);
    return write_text(path, depth);
}

static uint16_t pixel16(const p06_fb *fb, uint32_t x, uint32_t y)
{
    const unsigned char *p = fb->map + (size_t)y * fb->stride + (size_t)x * 2u;
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint16_t pack565(uint32_t rgb)
{
    return (uint16_t)((((rgb >> 19) & 0x1Fu) << 11) |
                      (((rgb >> 10) & 0x3Fu) << 5) |
                      ((rgb >> 3) & 0x1Fu));
}

/* ---------------------------------------------------------------- groups */

static void test_parser(void)
{
    printf("\n-- parser\n");

    static char line[LINEREADER_MAX * 2];
    p06_frame f;
    p06_drop why = P06_DROP_NONE;

    ok(flat_line(line, sizeof line, 1800, P06_ZONES), "a full frame line is built");
    ok(p06_parse_frame(line, &f, &why) == P06_OK, "it parses");
    ok(why == P06_DROP_NONE, "with no drop reason");
    ok(f.mm[0] == 1800 && f.mm[63] == 1800, "all sixty-four values arrived");

    ok(flat_line(line, sizeof line, 1800, P06_ZONES - 1), "a line one field short is built");
    ok(p06_parse_frame(line, &f, &why) != P06_OK, "it is refused");
    ok(why == P06_DROP_FIELD_COUNT, "and the reason is the field count");

    ok(flat_line(line, sizeof line, 1800, P06_ZONES + 1), "a line one field long is built");
    ok(p06_parse_frame(line, &f, &why) != P06_OK, "it is refused");
    ok(why == P06_DROP_FIELD_COUNT, "and the reason is the field count");

    ok(p06_parse_frame("INFO,boot complete", &f, &why) != P06_OK,
       "a line from another talker is refused");
    ok(why == P06_DROP_NOT_Z, "and the reason is that it is not a Z line");

    ok(p06_parse_frame("Z,1,2,x,4", &f, &why) != P06_OK, "rubbish in a field is refused");
    ok(why == P06_DROP_NOT_INTEGER, "and the reason is that it is not an integer");

    ok(p06_parse_frame("Z,1,-2,3", &f, &why) != P06_OK,
       "a negative distance is refused rather than read as a huge one");
    ok(why == P06_DROP_NOT_INTEGER, "and the reason is that it is not an integer");

    ok(flat_line(line, sizeof line, P06_RANGE_MAX_MM, P06_ZONES),
       "a frame at the sensor's maximum range is built");
    ok(p06_parse_frame(line, &f, &why) == P06_OK, "it parses");

    ok(flat_line(line, sizeof line, P06_RANGE_MAX_MM + 1, P06_ZONES),
       "a frame one millimetre beyond it is built");
    ok(p06_parse_frame(line, &f, &why) != P06_OK, "it is refused");
    ok(why == P06_DROP_OUT_OF_RANGE, "and the reason is the range");

    ok(flat_line(line, sizeof line, 0, P06_ZONES), "a frame of all zeroes is built");
    ok(p06_parse_frame(line, &f, &why) == P06_OK,
       "it parses, because zero is a reading and means no target");
}

static void test_frame(void)
{
    printf("\n-- frame\n");

    p06_frame f;
    for (uint16_t i = 0; i < P06_ZONES; i++)
        f.mm[i] = 1800;

    uint16_t with_target = 0;
    ok(p06_frame_near_mm(&f, &with_target) == 1800, "a flat wall is the nearest thing");
    ok(with_target == P06_ZONES, "and every zone saw it");

    f.mm[10] = 0;
    ok(p06_frame_near_mm(&f, &with_target) == 1800,
       "a zone with no target does not become the nearest thing");
    ok(with_target == P06_ZONES - 1, "and it is not counted as having seen one");

    f.mm[20] = 300;
    ok(p06_frame_near_mm(&f, NULL) == 300, "a hand is nearer than the wall");
    ok(p06_frame_far_mm(&f) == 1800, "and the wall is still the furthest");

    for (uint16_t i = 0; i < P06_ZONES; i++)
        f.mm[i] = 0;
    ok(p06_frame_near_mm(&f, &with_target) == 0, "a frame with nothing in it reads zero");
    ok(with_target == 0, "with no zone having seen a target");
}

static void test_book(void)
{
    printf("\n-- the book\n");

    p06_frame f;
    p06_book b;

    /* Too few frames. */
    p06_book_init(&b);
    for (uint16_t i = 0; i < P06_ZONES; i++)
        f.mm[i] = 1800;
    for (int i = 0; i < 5; i++)
        p06_book_add(&b, &f);
    ok(p06_book_finish(&b, P06_MARGIN_MM, P06_STABILITY_LIMIT_MM) != P06_OK,
       "five frames is refused as a baseline");

    /* A zone with no target. */
    p06_book_init(&b);
    f.mm[3] = 0;
    ok(p06_book_add(&b, &f) != P06_OK,
       "a frame with a zone that saw nothing is refused for a baseline");
    ok(b.frames == 0, "and it does not count towards the frames measured");
    f.mm[3] = 1800;

    /* A wall that moves more than the acceptance test allows. */
    p06_book_init(&b);
    for (int i = 0; i < 20; i++) {
        for (uint16_t z = 0; z < P06_ZONES; z++)
            f.mm[z] = (uint16_t)(1800 + ((i % 2) ? 30 : 0));
        p06_book_add(&b, &f);
    }
    ok(p06_book_finish(&b, P06_MARGIN_MM, P06_STABILITY_LIMIT_MM) != P06_OK,
       "a wall that moved 30 mm during the measurement is refused");
    ok(b.worst_spread_mm == 30, "and the spread it refused on is reported");

    /* A steady wall. */
    p06_book_init(&b);
    for (int i = 0; i < 20; i++) {
        for (uint16_t z = 0; z < P06_ZONES; z++)
            f.mm[z] = (uint16_t)(1800 + ((i % 2) ? 4 : 0));
        p06_book_add(&b, &f);
    }
    ok(p06_book_finish(&b, P06_MARGIN_MM, P06_STABILITY_LIMIT_MM) == P06_OK,
       "a wall steady within 4 mm gives a baseline");
    ok(b.worst_spread_mm == 4, "the spread is reported");
    ok(b.nearest_wall_mm == 1802, "the nearest wall is the mean of the two");
    ok(b.threshold_mm == 1802 - P06_MARGIN_MM,
       "and the threshold sits under it by the margin");

    snprintf(b.placement, sizeof b.placement, "bench, sensor facing the wall");
    ok(p06_book_write(&b, TMP_BOOK) == P06_OK, "the book is written");

    p06_book back;
    ok(p06_book_read(&back, TMP_BOOK) == P06_OK, "and read");
    ok(back.threshold_mm == b.threshold_mm, "the threshold survives the round trip");
    ok(back.nearest_wall_mm == b.nearest_wall_mm, "the wall survives it");
    ok(back.worst_spread_mm == b.worst_spread_mm, "the spread survives it");
    ok(back.wall_mm[0] == b.wall_mm[0] && back.wall_mm[63] == b.wall_mm[63],
       "and so does the per-zone wall, at both ends");
    ok(strcmp(back.placement, "bench, sensor facing the wall") == 0,
       "the placement survives, because a threshold without one is a number");

    /* Classification, including the zero that is not a target. */
    for (uint16_t z = 0; z < P06_ZONES; z++)
        f.mm[z] = 1800;
    f.mm[5] = 300;
    f.mm[6] = 0;
    p06_occupancy o;
    ok(p06_classify(&f, &back, &o) == P06_OK, "a frame is classified");
    ok(o.count == 1, "the hand occupies one zone");
    ok(o.occupied[5] && !o.occupied[6],
       "and the zone that saw nothing is not occupied");
}

static void test_glass(void)
{
    printf("\n-- the glass\n");

    p06_geometry g;
    ok(p06_geometry_parse(&g, "480x320x16") == P06_OK, "a geometry string parses");
    ok(g.width == 480 && g.height == 320 && g.bpp == 16, "and gives the three numbers");
    ok(p06_geometry_parse(&g, "480x320x24") != P06_OK,
       "twenty-four bits per pixel is refused by name rather than painted wrongly");
    ok(p06_geometry_parse(&g, "nonsense") != P06_OK, "and nonsense is refused");

    ok(write_sysfs(TMP_SYSFS, "480,320\n", "16\n"), "a sysfs directory is written");
    ok(p06_geometry_from_sysfs(&g, TMP_SYSFS) == P06_OK, "the geometry is read from it");
    ok(g.width == 480 && g.height == 320 && g.bpp == 16,
       "and matches what the kernel would have published");

    /* A small display, so that the guard area is close to the painting. */
    p06_geometry small = {32, 24, 16};
    const size_t bytes = 32u * 24u * 2u;
    ok(make_glass(TMP_GLASS, bytes, GUARD_BYTES), "a glass file with a guard area is made");

    p06_fb fb;
    p06_fb_init(&fb);
    ok(p06_fb_open(&fb, TMP_GLASS, &small) == P06_OK, "it opens");
    ok(fb.bytes == bytes, "the mapping is exactly the geometry, not the file");

    p06_frame f;
    for (uint16_t z = 0; z < P06_ZONES; z++)
        f.mm[z] = 1800;
    ok(p06_fb_paint(&fb, &f, NULL) == P06_OK, "a frame paints");

    /* The rectangles that cannot be allowed to reach the guard area. */
    ok(p06_fb_fill(&fb, 30, 22, 100, 100, 0xFFFFFFu) == P06_OK,
       "a rectangle overhanging the bottom right is clipped, not refused");
    ok(p06_fb_fill(&fb, 1000, 1000, 10, 10, 0xFFFFFFu) == P06_OK,
       "and one entirely outside paints nothing and is not an error");
    ok(p06_fb_close(&fb) == P06_OK, "the glass closes");
    ok(guard_intact(TMP_GLASS, bytes, GUARD_BYTES),
       "every byte past the geometry is still untouched");

    /* The colour of a known zone, at 16 bits. */
    p06_geometry panel = {480, 320, 16};
    const size_t panel_bytes = 480u * 320u * 2u;
    ok(make_glass(TMP_GLASS, panel_bytes, GUARD_BYTES), "a panel sized glass is made");
    p06_fb_init(&fb);
    ok(p06_fb_open(&fb, TMP_GLASS, &panel) == P06_OK, "it opens");

    p06_book b;
    p06_book_init(&b);
    b.threshold_mm = 1000;
    b.nearest_wall_mm = 1800;
    for (uint16_t z = 0; z < P06_ZONES; z++)
        f.mm[z] = 1800;
    f.mm[0] = 300;    /* top left, occupied */
    f.mm[9] = 0;      /* second row, second column, nothing seen */

    ok(p06_fb_paint(&fb, &f, &b) == P06_OK, "a classified frame paints");
    ok(pixel16(&fb, 0, 0) == pack565(p06_zone_colour(300, &b)),
       "the top left pixel carries the occupied colour");
    ok(pixel16(&fb, 59, 39) == pack565(p06_zone_colour(300, &b)),
       "and so does the far corner of that same cell");
    ok(pixel16(&fb, 60, 0) != pack565(p06_zone_colour(300, &b)),
       "the next cell along does not");
    ok(pixel16(&fb, 61, 41) == pack565(p06_zone_colour(0, &b)),
       "the zone that saw nothing is painted as having seen nothing");
    ok(p06_fb_close(&fb) == P06_OK, "the glass closes");
    ok(guard_intact(TMP_GLASS, panel_bytes, GUARD_BYTES),
       "and nothing was written past the panel");

    /* A file too short for the geometry it is given. */
    ok(make_glass(TMP_GLASS, 100, 0), "a glass file far too small is made");
    p06_fb_init(&fb);
    ok(p06_fb_open(&fb, TMP_GLASS, &small) != P06_OK,
       "opening it with a larger geometry is refused rather than mapped");
}

static void test_synthesis(void)
{
    printf("\n-- synthesis\n");

    p06_synth s;
    p06_synth_init(&s);
    s.frames = 12;
    ok(p06_synth_write(&s, TMP_STREAM) == P06_OK, "an empty field stream is written");
    ok(p06_synth_write(&s, TMP_OTHER) == P06_OK, "and written again");

    static char a[65536];
    static char c[65536];
    FILE *fp = fopen(TMP_STREAM, "rb");
    size_t na = fp ? fread(a, 1, sizeof a - 1, fp) : 0;
    if (fp) fclose(fp);
    fp = fopen(TMP_OTHER, "rb");
    size_t nc = fp ? fread(c, 1, sizeof c - 1, fp) : 0;
    if (fp) fclose(fp);
    ok(na > 0 && na == nc && memcmp(a, c, na) == 0,
       "the same request twice gives the same bytes, to the byte");

    p06_run run;
    p06_run_init(&run);
    ok(p06_run_stream(TMP_STREAM, NULL, NULL, 0, &run) == P06_OK, "it is read back");
    ok(run.accepted == 12, "all twelve frames are accepted");
    ok(p06_run_dropped_total(&run) == 0, "and nothing is dropped");

    /* One fault of each kind, each in its own frame. */
    p06_synth_init(&s);
    s.frames = 12;
    ok(p06_synth_add_fault(&s, P06_FAULT_SHORT, 2) == P06_OK, "a short line is placed");
    ok(p06_synth_add_fault(&s, P06_FAULT_NOT_Z, 3) == P06_OK, "a line from elsewhere");
    ok(p06_synth_add_fault(&s, P06_FAULT_RUBBISH, 4) == P06_OK, "a rubbish field");
    ok(p06_synth_add_fault(&s, P06_FAULT_HUGE, 5) == P06_OK, "an impossible distance");
    ok(p06_synth_add_fault(&s, P06_FAULT_LONG, 6) == P06_OK, "an over-long line");
    ok(p06_synth_write(&s, TMP_OTHER) == P06_OK, "the faulty stream is written");

    p06_run_init(&run);
    ok(p06_run_stream(TMP_OTHER, NULL, NULL, 0, &run) == P06_OK, "it is read");
    ok(run.accepted == 7, "seven of the twelve frames survive");
    ok(run.dropped[P06_DROP_FIELD_COUNT] == 1, "the short line is one field count drop");
    ok(run.dropped[P06_DROP_NOT_Z] == 1, "the other talker is one not-a-Z drop");
    ok(run.dropped[P06_DROP_NOT_INTEGER] == 1, "the rubbish field is one integer drop");
    ok(run.dropped[P06_DROP_OUT_OF_RANGE] == 1, "the impossible distance is one range drop");
    ok(run.long_lines == 1, "and the over-long line is refused by the reader");
    ok(p06_run_dropped_total(&run) == 4,
       "four drops in the parser, with the fifth refused before it got there");
}

static void test_the_claim(void)
{
    printf("\n-- nothing is painted that was not parsed\n");

    /* A stream of twelve frames with five bad lines in it, painted into a
     * file. The count that matters is not how many frames arrived but how
     * many times the glass was written. */
    p06_synth s;
    p06_synth_init(&s);
    s.frames = 12;
    s.scene = P06_SCENE_HAND;
    s.hand_zones = 6;
    p06_synth_add_fault(&s, P06_FAULT_SHORT, 2);
    p06_synth_add_fault(&s, P06_FAULT_NOT_Z, 3);
    p06_synth_add_fault(&s, P06_FAULT_RUBBISH, 4);
    p06_synth_add_fault(&s, P06_FAULT_HUGE, 5);
    p06_synth_add_fault(&s, P06_FAULT_LONG, 6);
    ok(p06_synth_write(&s, TMP_STREAM) == P06_OK, "a hand stream with five bad lines");

    p06_geometry small = {32, 24, 16};
    const size_t bytes = 32u * 24u * 2u;
    ok(make_glass(TMP_GLASS, bytes, GUARD_BYTES), "a glass file is made");

    p06_fb fb;
    p06_fb_init(&fb);
    ok(p06_fb_open(&fb, TMP_GLASS, &small) == P06_OK, "it opens");

    p06_run run;
    p06_run_init(&run);
    ok(p06_run_stream(TMP_STREAM, NULL, &fb, 0, &run) == P06_OK, "the stream is run");
    ok(run.accepted == 7, "seven frames were parsed");
    ok(run.painted == 7, "and the glass was written exactly seven times");
    ok(p06_fb_close(&fb) == P06_OK, "the glass closes");
    ok(guard_intact(TMP_GLASS, bytes, GUARD_BYTES), "with nothing past the geometry");

    /* A stream with nothing in it but a bad line leaves the glass alone. */
    ok(write_text(TMP_OTHER, "INFO,still booting\n"), "a stream with no frame in it");
    ok(make_glass(TMP_GLASS, bytes, GUARD_BYTES), "a fresh glass file");

    p06_fb_init(&fb);
    ok(p06_fb_open(&fb, TMP_GLASS, &small) == P06_OK, "it opens");
    p06_run_init(&run);
    ok(p06_run_stream(TMP_OTHER, NULL, &fb, 0, &run) == P06_OK, "the stream is run");
    ok(run.accepted == 0 && run.painted == 0, "nothing was parsed and nothing painted");
    ok(p06_fb_close(&fb) == P06_OK, "the glass closes");
    /* From the first byte rather than from the end of the geometry: with no
     * frame parsed, the whole file must be as it was made, not merely its
     * guard area. */
    ok(guard_intact(TMP_GLASS, 0, bytes + GUARD_BYTES),
       "and the glass is byte for byte as it was, from the first byte");

    /* The hand reads as the hand, and an empty field does not. */
    p06_synth_init(&s);
    s.frames = 20;
    s.scene = P06_SCENE_EMPTY;
    ok(p06_synth_write(&s, TMP_STREAM) == P06_OK, "an empty field stream");

    p06_book b;
    p06_book_init(&b);
    p06_frame f;
    for (int i = 0; i < 20; i++) {
        for (uint16_t z = 0; z < P06_ZONES; z++)
            f.mm[z] = 1800;
        p06_book_add(&b, &f);
    }
    ok(p06_book_finish(&b, P06_MARGIN_MM, P06_STABILITY_LIMIT_MM) == P06_OK,
       "a baseline is taken");

    p06_run_init(&run);
    ok(p06_run_stream(TMP_STREAM, &b, NULL, 0, &run) == P06_OK, "the empty field is run");
    ok(run.max_occupied == 0, "no zone in an empty field is ever occupied");

    p06_synth_init(&s);
    s.frames = 20;
    s.scene = P06_SCENE_HAND;
    s.hand_zones = 6;
    ok(p06_synth_write(&s, TMP_OTHER) == P06_OK, "a hand stream");
    p06_run_init(&run);
    ok(p06_run_stream(TMP_OTHER, &b, NULL, 0, &run) == P06_OK, "the hand is run");
    ok(run.max_occupied == 6, "the hand occupies exactly the six zones it covers");
    ok(run.min_occupied == 6, "on every frame of the run, not just the busiest");
    ok(run.max_occupied >= 4 && run.max_occupied <= 10,
       "which is inside the 4 to 10 the acceptance test asks for");
}

static void cleanup(void)
{
    remove(TMP_STREAM);
    remove(TMP_OTHER);
    remove(TMP_BOOK);
    remove(TMP_GLASS);
    char path[512];
    snprintf(path, sizeof path, "%s/virtual_size", TMP_SYSFS);
    remove(path);
    snprintf(path, sizeof path, "%s/bits_per_pixel", TMP_SYSFS);
    remove(path);
    remove(TMP_SYSFS);
}

int main(void)
{
    printf("P06 suite: no hardware, no display, no sensor\n");

    test_parser();
    test_frame();
    test_book();
    test_glass();
    test_synthesis();
    test_the_claim();
    cleanup();

    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    if (g_failed > 0)
        return 1;
    printf("No distance here came from a sensor. Every frame was written by\n"
           "this suite a moment earlier.\n");
    return 0;
}
