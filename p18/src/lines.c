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

/* The reader is lib/linereader.h now. These three forward to it and map its
 * status onto this program's, which is the whole cost of sharing it. The
 * refusal that matters, an over-long line discarded whole rather than handed
 * back in two pieces, is described where it now lives. */

static p18_status from_lr(linereader_status s)
{
    switch (s) {
    case LINEREADER_OK:  return P18_OK;
    case LINEREADER_EOF: return P18_E_EOF;
    case LINEREADER_IO:  return P18_E_IO;
    case LINEREADER_ARG: return P18_E_ARG;
    }
    return P18_E_IO;
}

p18_status p18_reader_open(p18_reader *r, const char *path)
{
    return from_lr(linereader_open(r, path));
}

void p18_reader_close(p18_reader *r)
{
    linereader_close(r);
}

p18_status p18_reader_line(p18_reader *r, const char **line, size_t *len)
{
    return from_lr(linereader_line(r, line, len));
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
