/* Reading lines from a stream that may be a file or a character device, and
 * reading the one field this program interprets.
 *
 * Nothing here knows what the payload means. The firmware decides that, and
 * the chapter is explicit that a recorder which starts interpreting the
 * stream has stopped being a recorder.
 */

#include "p18.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

const char *p18_strerror(p18_status s)
{
    switch (s) {
    case P18_OK:        return "ok";
    case P18_E_IO:      return "the file could not be read or written";
    case P18_E_FORMAT:  return "the content is not what this program writes";
    case P18_E_RANGE:   return "a value is outside the range this program allows";
    case P18_E_ARG:     return "an argument is missing or will not do";
    case P18_E_EOF:     return "the end of the stream";
    }
    return "unknown";
}

p18_status p18_reader_open(p18_reader *r, const char *path)
{
    if (r == NULL || path == NULL)
        return P18_E_ARG;
    memset(r, 0, sizeof *r);
    if (strcmp(path, "-") == 0) {
        r->fp = stdin;
        r->owns_fp = false;
        return P18_OK;
    }
    r->fp = fopen(path, "rb");
    if (r->fp == NULL)
        return P18_E_IO;
    r->owns_fp = true;
    return P18_OK;
}

void p18_reader_close(p18_reader *r)
{
    if (r == NULL || r->fp == NULL)
        return;
    if (r->owns_fp)
        fclose(r->fp);
    r->fp = NULL;
}

p18_status p18_reader_line(p18_reader *r, const char **line, size_t *len)
{
    if (r == NULL || r->fp == NULL || line == NULL || len == NULL)
        return P18_E_ARG;

    for (;;) {
        if (fgets(r->buf, (int)sizeof r->buf, r->fp) == NULL)
            return feof(r->fp) ? P18_E_EOF : P18_E_IO;

        size_t n = strlen(r->buf);
        bool terminated = (n > 0 && r->buf[n - 1] == '\n');

        if (!terminated && !feof(r->fp)) {
            /* It did not fit. Discard the whole line rather than return the
             * part that did: the remainder would come back as a line of its
             * own and its first field would parse as a sequence number that
             * was never sent. Losing a sample is a measurement. Inventing one
             * is a lie in the same column. */
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
        return P18_OK;
    }
}

p18_status p18_parse_seq(const char *line, uint32_t *seq)
{
    if (line == NULL || seq == NULL)
        return P18_E_ARG;

    /* No sign is accepted. strtoull would take a leading minus and wrap it
     * into a very large number, which would read here as a reorder across
     * most of the range rather than as the malformed line it is. */
    if (line[0] < '0' || line[0] > '9')
        return P18_E_FORMAT;

    errno = 0;
    char *end = NULL;
    unsigned long long v = strtoull(line, &end, 10);
    if (end == line || errno == ERANGE)
        return P18_E_FORMAT;
    if (v > 0xFFFFFFFFull)
        return P18_E_RANGE;

    /* The field ends where the payload begins. Anything else glued to the
     * digits means the line is not the shape this reader was promised. */
    if (*end != '\0' && *end != ',' && *end != ' ' && *end != '\t')
        return P18_E_FORMAT;

    *seq = (uint32_t)v;
    return P18_OK;
}
