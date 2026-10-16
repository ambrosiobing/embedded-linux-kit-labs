/* The loop, which is the lab.
 *
 * Read a line, parse it or drop it with a reason, classify it against the
 * measured wall, paint it. The paint is inside this loop and nowhere else,
 * which is the chapter's instruction stated as code: a sensor that stops
 * sending stops the repaint, rather than leaving a frame on the glass that
 * is a minute old and looks live. There is no path through this file that
 * paints anything that was not parsed in the same iteration.
 */

#include "p06.h"

#include <string.h>

void p06_run_init(p06_run *run)
{
    if (run == NULL)
        return;
    memset(run, 0, sizeof *run);
}

uint32_t p06_run_dropped_total(const p06_run *run)
{
    if (run == NULL)
        return 0;
    uint32_t total = 0;
    for (size_t i = 0; i < sizeof run->dropped / sizeof run->dropped[0]; i++)
        total += run->dropped[i];
    return total;
}

p06_status p06_run_stream(const char *source, const p06_book *b, p06_fb *fb,
                          uint32_t max_frames, p06_run *run)
{
    if (source == NULL || run == NULL)
        return P06_E_ARG;

    linereader r;
    if (linereader_open(&r, source) != LINEREADER_OK)
        return P06_E_IO;

    p06_status st = P06_OK;
    p06_frame f;
    p06_occupancy o;

    for (;;) {
        const char *line = NULL;
        size_t len = 0;
        linereader_status ls = linereader_line(&r, &line, &len);
        if (ls == LINEREADER_EOF)
            break;
        if (ls != LINEREADER_OK) {
            st = P06_E_IO;
            break;
        }

        p06_drop why = P06_DROP_NONE;
        if (p06_parse_frame(line, &f, &why) != P06_OK) {
            if ((size_t)why < sizeof run->dropped / sizeof run->dropped[0])
                run->dropped[why]++;
            continue;
        }

        run->accepted++;

        uint16_t near = p06_frame_near_mm(&f, NULL);
        if (!run->started) {
            run->started = true;
            run->near_min_mm = near;
            run->near_max_mm = near;
        } else {
            if (near < run->near_min_mm)
                run->near_min_mm = near;
            if (near > run->near_max_mm)
                run->near_max_mm = near;
        }

        if (b != NULL && b->threshold_mm > 0) {
            if (p06_classify(&f, b, &o) == P06_OK) {
                run->last_occupied = o.count;
                if (run->accepted == 1) {
                    run->min_occupied = o.count;
                    run->max_occupied = o.count;
                } else {
                    if (o.count < run->min_occupied)
                        run->min_occupied = o.count;
                    if (o.count > run->max_occupied)
                        run->max_occupied = o.count;
                }
            }
        }

        if (fb != NULL) {
            st = p06_fb_paint(fb, &f, b);
            if (st != P06_OK)
                break;
            run->painted++;
        }

        if (max_frames != 0 && run->accepted >= max_frames)
            break;
    }

    run->long_lines = r.long_lines;
    run->blank_lines = r.blank_lines;
    linereader_close(&r);
    return st;
}
