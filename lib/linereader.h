/* Reading lines from a stream that may be a file, a pipe or a character
 * device, shared by the labs that record one.
 *
 * It is fgets with one thing added, and that thing is the reason this file
 * exists rather than a call to fgets at each site: a line that does not fit
 * the buffer is refused whole.
 *
 * fgets hands back the part that fitted and then the remainder on the next
 * call, and the remainder is indistinguishable from a line the device sent.
 * In P18 it arrives as a sample nobody sent. In P06 it arrives as a partial
 * frame, which painted on the glass looks like a real reading and is not.
 * Both labs already had a rule against exactly that, written in prose, and
 * prose does not run. So the reader counts the over-long line, discards it to
 * the next newline, and goes on.
 *
 * A final line that fills the buffer exactly and ends at end of file with no
 * newline is counted as over-long too. That is conservative, it is written
 * down here rather than left to be discovered, and no stream either lab reads
 * produces lines anywhere near the buffer size.
 *
 * Blank lines are skipped and counted. Carriage returns are stripped with the
 * newline, because a device configured by someone else may send either.
 *
 * Header only, with static inline functions, like the other file in here.
 * There is no library to build and no object file for two programs to collide
 * over. The buffer is part of the struct, so a reader is a fixed-size object
 * a caller can put on the stack or in a static, and nothing in it allocates.
 */
#ifndef LINEREADER_H
#define LINEREADER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Long enough for a sequence number and a payload, or for a line of 65
 * comma-separated fields, and short enough that a device sending binary
 * rubbish is noticed rather than buffered. */
#define LINEREADER_MAX 512

typedef enum {
    LINEREADER_OK = 0,
    LINEREADER_EOF,
    LINEREADER_IO,
    LINEREADER_ARG
} linereader_status;

typedef struct {
    FILE *fp;
    bool owns_fp;
    char buf[LINEREADER_MAX + 1];
    uint32_t long_lines;
    uint32_t blank_lines;
} linereader;

/* A path of "-" is standard input, and is not closed on the way out. */
static inline linereader_status linereader_open(linereader *r, const char *path)
{
    if (r == NULL || path == NULL)
        return LINEREADER_ARG;
    memset(r, 0, sizeof *r);
    if (strcmp(path, "-") == 0) {
        r->fp = stdin;
        r->owns_fp = false;
        return LINEREADER_OK;
    }
    r->fp = fopen(path, "rb");
    if (r->fp == NULL)
        return LINEREADER_IO;
    r->owns_fp = true;
    return LINEREADER_OK;
}

static inline void linereader_close(linereader *r)
{
    if (r == NULL || r->fp == NULL)
        return;
    if (r->owns_fp)
        fclose(r->fp);
    r->fp = NULL;
}

/* On LINEREADER_OK, *line points into the reader's own buffer and is valid
 * until the next call. */
static inline linereader_status linereader_line(linereader *r,
                                                const char **line, size_t *len)
{
    if (r == NULL || r->fp == NULL || line == NULL || len == NULL)
        return LINEREADER_ARG;

    for (;;) {
        if (fgets(r->buf, (int)sizeof r->buf, r->fp) == NULL)
            return feof(r->fp) ? LINEREADER_EOF : LINEREADER_IO;

        size_t n = strlen(r->buf);
        bool terminated = (n > 0 && r->buf[n - 1] == '\n');

        if (!terminated && !feof(r->fp)) {
            r->long_lines++;
            int c;
            while ((c = fgetc(r->fp)) != EOF && c != '\n')
                ;
            continue;
        }

        while (n > 0 && (r->buf[n - 1] == '\n' || r->buf[n - 1] == '\r'))
            r->buf[--n] = '\0';

        if (n == 0) {
            r->blank_lines++;
            continue;
        }
        *line = r->buf;
        *len = n;
        return LINEREADER_OK;
    }
}

#endif /* LINEREADER_H */
