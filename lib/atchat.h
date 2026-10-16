/* Talking to a module that answers in text, shared by the labs that do.
 *
 * Both cellular labs in this volume drive their radio with AT commands, and
 * both chapters showed the same host loop: write a command, sleep a fixed
 * time, read whatever happened to arrive. That loop is wrong in a way that
 * works on the bench and fails in the field. It does not wait for the module
 * to finish; it waits for a number somebody guessed. A reply that takes
 * longer than the guess is read as the next command's reply, and from then
 * on every answer in the session belongs to the question before it.
 *
 * So this waits for a final line instead: OK, ERROR, a +CME or +CMS error, or
 * the prompt a module gives when it wants a payload. A deadline bounds the
 * wait, and a deadline that expires is a timeout rather than an empty reply,
 * because those two mean different things and only one of them is the
 * module's fault.
 *
 * Two back ends, one engine.
 *
 *   serial      a character device, configured through termios, read with a
 *               deadline through poll
 *   transcript  a file of what a module said, and what the host is expected
 *               to have asked
 *
 * The transcript is the reason these labs can be finished before the HATs are
 * seated, and it is more than a stub. Each command the program sends is
 * checked against the one the transcript records, so replaying a session
 * tests the order of the conversation and not just its arithmetic. P03's
 * whole claim is an ordering, that the GNSS engine is powered down before the
 * radio transmits, and a transcript replay is that claim made executable with
 * no antenna in the room.
 *
 * Nothing here allocates. The response buffer is part of the struct.
 */
#ifndef ATCHAT_H
#define ATCHAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define AT_LINE_MAX 256
#define AT_RESPONSE_MAX 2048
#define AT_TIMEOUT_DEFAULT_MS 2000

typedef enum {
    AT_OK = 0,
    AT_E_IO,
    AT_E_TIMEOUT,     /* the module said nothing in time */
    AT_E_ERROR,       /* the module said ERROR, or a +CME or +CMS error */
    AT_E_ARG,
    AT_E_FORMAT,
    AT_E_MISMATCH,    /* the transcript expected a different command */
    AT_E_EOF          /* the transcript ran out */
} at_status;

const char *at_strerror(at_status s);

typedef enum {
    AT_FINAL_NONE = 0,
    AT_FINAL_OK,
    AT_FINAL_ERROR,
    AT_FINAL_PROMPT   /* the module wants a payload, then a Ctrl-Z */
} at_final;

typedef enum {
    AT_CHANNEL_NONE = 0,
    AT_CHANNEL_SERIAL,
    AT_CHANNEL_TRANSCRIPT
} at_channel;

typedef struct {
    at_channel kind;
    int fd;                 /* serial */
    FILE *script;           /* transcript */
    FILE *log;              /* every exchange, in transcript form. May be NULL */
    unsigned timeout_ms;

    char resp[AT_RESPONSE_MAX];   /* the reply, without the final line */
    size_t resp_len;
    char final_line[AT_LINE_MAX];
    at_final final;

    /* What the session did, for the lab book. */
    uint32_t commands;
    uint32_t timeouts;
    uint32_t errors;
} atchat;

void atchat_init(atchat *a);

/* 115200 is what both HATs present. Other rates are refused rather than
 * silently approximated, because a baud this program cannot name is a baud
 * nobody has tested it at. */
at_status atchat_open_serial(atchat *a, const char *path, unsigned baud);
at_status atchat_open_transcript(atchat *a, const char *path);
at_status atchat_close(atchat *a);

/* Write every exchange to this file in the transcript format, so that a
 * session with a real module can be replayed afterwards with none. */
at_status atchat_log_to(atchat *a, const char *path);

/* Send a command and wait for a final line. The command is written without
 * its terminator, which this adds. */
at_status atchat_command(atchat *a, const char *cmd);
at_status atchat_command_timeout(atchat *a, const char *cmd, unsigned timeout_ms);

/* Send a payload after a prompt, terminated with Ctrl-Z, and wait for the
 * final line that follows it. */
at_status atchat_payload(atchat *a, const char *payload);

/* The reply to the last command, without its final line. */
const char *atchat_response(const atchat *a);

/* The first reply line beginning with prefix, or NULL. The pointer is into
 * the response buffer and is valid until the next command. */
const char *atchat_find(const atchat *a, const char *prefix);

/* The nth comma-separated field of a line, copied out and trimmed. Fields are
 * numbered from zero after the prefix, so in
 *
 *     +CGNSINF: 1,1,20260916103012.000,47.07,15.44
 *
 * field 0 is "1" and field 2 is "20260916103012.000". Returns false when the
 * line has no such field, which is the common case with this family of
 * modules and is not an error. */
bool at_field(const char *line, unsigned n, char *out, size_t cap);

#endif /* ATCHAT_H */
