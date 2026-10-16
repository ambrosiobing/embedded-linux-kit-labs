/* The AT engine. See atchat.h for why it waits for a final line rather than
 * for a number somebody guessed.
 *
 * The transcript format, which is also what the logger writes:
 *
 *     # a comment
 *     > AT+CSQ            what the host is expected to send
 *     < +CSQ: 19,99       what the module replies
 *     < OK
 *     ! timeout           the module says nothing until the deadline
 *
 * A command that does not match the recorded one is AT_E_MISMATCH rather than
 * a warning. That is the point of replaying a session: the order of the
 * conversation is part of what is being tested.
 */
/* _DEFAULT_SOURCE as well as the POSIX one, and deliberately. The three
 * things this file needs that strict POSIX does not define are B115200,
 * CRTSCTS and cfmakeraw, and all three are what every Linux serial program
 * uses. A port configured by a program that avoided them would be a port
 * configured by guesswork. */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "atchat.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

const char *at_strerror(at_status s)
{
    switch (s) {
    case AT_OK:         return "ok";
    case AT_E_IO:       return "the port could not be read or written";
    case AT_E_TIMEOUT:  return "the module said nothing in time";
    case AT_E_ERROR:    return "the module refused the command";
    case AT_E_ARG:      return "an argument is missing or will not do";
    case AT_E_FORMAT:   return "the transcript is not in the expected form";
    case AT_E_MISMATCH: return "the command sent is not the command recorded";
    case AT_E_EOF:      return "the transcript ended";
    }
    return "unknown";
}

void atchat_init(atchat *a)
{
    if (a == NULL)
        return;
    memset(a, 0, sizeof *a);
    a->fd = -1;
    a->timeout_ms = AT_TIMEOUT_DEFAULT_MS;
}

/* ---------------------------------------------------------------- serial */

at_status atchat_open_serial(atchat *a, const char *path, unsigned baud)
{
    if (a == NULL || path == NULL)
        return AT_E_ARG;
    if (baud != 115200)
        return AT_E_ARG;   /* named rather than approximated: see the header */

    atchat_init(a);
    a->fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (a->fd < 0)
        return AT_E_IO;

    struct termios t;
    if (tcgetattr(a->fd, &t) != 0) {
        close(a->fd);
        a->fd = -1;
        return AT_E_IO;
    }
    cfmakeraw(&t);
    t.c_cflag |= (CLOCAL | CREAD);
    t.c_cflag &= ~(tcflag_t)CRTSCTS;
    t.c_cflag &= ~(tcflag_t)CSTOPB;
    t.c_cflag &= ~(tcflag_t)PARENB;
    /* The deadline is in poll, not here, so the read itself never blocks. */
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    if (cfsetispeed(&t, B115200) != 0 || cfsetospeed(&t, B115200) != 0 ||
        tcsetattr(a->fd, TCSANOW, &t) != 0) {
        close(a->fd);
        a->fd = -1;
        return AT_E_IO;
    }
    tcflush(a->fd, TCIOFLUSH);
    a->kind = AT_CHANNEL_SERIAL;
    return AT_OK;
}

at_status atchat_open_transcript(atchat *a, const char *path)
{
    if (a == NULL || path == NULL)
        return AT_E_ARG;
    atchat_init(a);
    a->script = fopen(path, "rb");
    if (a->script == NULL)
        return AT_E_IO;
    a->kind = AT_CHANNEL_TRANSCRIPT;
    return AT_OK;
}

at_status atchat_log_to(atchat *a, const char *path)
{
    if (a == NULL || path == NULL)
        return AT_E_ARG;
    a->log = fopen(path, "wb");
    if (a->log == NULL)
        return AT_E_IO;
    fprintf(a->log, "# An AT session, recorded so that it can be replayed\n"
                    "# with no module attached: p0x replay --transcript this\n");
    return AT_OK;
}

at_status atchat_close(atchat *a)
{
    if (a == NULL)
        return AT_E_ARG;
    at_status s = AT_OK;
    if (a->fd >= 0 && close(a->fd) != 0)
        s = AT_E_IO;
    if (a->script != NULL && fclose(a->script) != 0)
        s = AT_E_IO;
    if (a->log != NULL && fclose(a->log) != 0)
        s = AT_E_IO;
    a->fd = -1;
    a->script = NULL;
    a->log = NULL;
    a->kind = AT_CHANNEL_NONE;
    return s;
}

/* ---------------------------------------------------------------- lines */

static void trim(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' '))
        s[--n] = '\0';
}

static at_final classify(const char *line)
{
    if (strcmp(line, "OK") == 0)
        return AT_FINAL_OK;
    if (strcmp(line, "ERROR") == 0)
        return AT_FINAL_ERROR;
    if (strncmp(line, "+CME ERROR:", 11) == 0)
        return AT_FINAL_ERROR;
    if (strncmp(line, "+CMS ERROR:", 11) == 0)
        return AT_FINAL_ERROR;
    /* A bare greater-than is the prompt these modules give before a payload.
     * It arrives with no newline of its own, which the readers below handle
     * by treating it as a line in its own right. */
    if (strcmp(line, ">") == 0)
        return AT_FINAL_PROMPT;
    return AT_FINAL_NONE;
}

static void keep(atchat *a, const char *line)
{
    size_t n = strlen(line);
    if (a->resp_len + n + 2 >= sizeof a->resp)
        return;   /* a reply longer than the buffer keeps its first part */
    memcpy(a->resp + a->resp_len, line, n);
    a->resp_len += n;
    a->resp[a->resp_len++] = '\n';
    a->resp[a->resp_len] = '\0';
}

static void log_line(atchat *a, char direction, const char *text)
{
    if (a->log != NULL)
        fprintf(a->log, "%c %s\n", direction, text);
}

/* ---------------------------------------------------------------- serial io */

static long now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

/* One line, or AT_E_TIMEOUT when the deadline passes with none. A bare
 * prompt has no newline after it, so a buffer holding exactly "> " is
 * returned as a line once nothing more arrives. */
static at_status serial_line(atchat *a, char *out, size_t cap, long deadline)
{
    size_t n = 0;
    for (;;) {
        long left = deadline - now_ms();
        if (left <= 0) {
            if (n > 0) {
                out[n] = '\0';
                trim(out);
                return AT_OK;
            }
            return AT_E_TIMEOUT;
        }
        struct pollfd p;
        p.fd = a->fd;
        p.events = POLLIN;
        p.revents = 0;
        int r = poll(&p, 1, (int)(left > 100 ? 100 : left));
        if (r < 0) {
            if (errno == EINTR)
                continue;
            return AT_E_IO;
        }
        if (r == 0) {
            /* Nothing yet. A prompt that arrived without a newline is a
             * complete answer once the module has stopped talking. */
            if (n >= 1 && out[n - 1] == '>') {
                out[n] = '\0';
                trim(out);
                return AT_OK;
            }
            continue;
        }
        char c;
        ssize_t got = read(a->fd, &c, 1);
        if (got < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return AT_E_IO;
        }
        if (got == 0)
            continue;
        if (c == '\n') {
            out[n] = '\0';
            trim(out);
            if (out[0] == '\0') {
                n = 0;      /* blank line between a command and its reply */
                continue;
            }
            return AT_OK;
        }
        if (n + 1 < cap)
            out[n++] = c;
    }
}

/* ---------------------------------------------------------------- transcript */

/* The next directive line of the transcript, with its marker.
 *
 * A line longer than a line is refused rather than truncated, for the same
 * reason lib/linereader.h refuses one: a transcript cut in the middle would
 * replay as a command nobody recorded. The compiler asked the question first,
 * on the copy that used to truncate silently. */
static at_status script_line(atchat *a, char *marker, char *out, size_t cap)
{
    char raw[AT_LINE_MAX + 8];
    for (;;) {
        if (fgets(raw, (int)sizeof raw, a->script) == NULL)
            return AT_E_EOF;

        size_t n = strlen(raw);
        if (n > 0 && raw[n - 1] != '\n' && !feof(a->script)) {
            int c;
            while ((c = fgetc(a->script)) != EOF && c != '\n')
                ;
            return AT_E_FORMAT;
        }

        trim(raw);
        if (raw[0] == '\0' || raw[0] == '#')
            continue;
        if (raw[1] != ' ' && raw[1] != '\0')
            return AT_E_FORMAT;

        const char *body = (raw[1] == '\0') ? "" : raw + 2;
        size_t blen = strlen(body);
        if (blen + 1 > cap)
            return AT_E_FORMAT;

        *marker = raw[0];
        memcpy(out, body, blen);
        out[blen] = '\0';
        return AT_OK;
    }
}

/* ---------------------------------------------------------------- commands */

static at_status read_until_final(atchat *a, unsigned timeout_ms)
{
    a->resp[0] = '\0';
    a->resp_len = 0;
    a->final_line[0] = '\0';
    a->final = AT_FINAL_NONE;

    if (a->kind == AT_CHANNEL_SERIAL) {
        const long deadline = now_ms() + (long)timeout_ms;
        char line[AT_LINE_MAX];
        for (;;) {
            at_status s = serial_line(a, line, sizeof line, deadline);
            if (s == AT_E_TIMEOUT) {
                /* Logged as the directive a transcript uses, so that a
                 * recorded silence replays as a silence rather than as the
                 * end of the file. */
                if (a->log != NULL)
                    fprintf(a->log, "! timeout\n");
                a->timeouts++;
                return AT_E_TIMEOUT;
            }
            if (s != AT_OK)
                return s;
            log_line(a, '<', line);
            at_final f = classify(line);
            if (f == AT_FINAL_NONE) {
                keep(a, line);
                continue;
            }
            snprintf(a->final_line, sizeof a->final_line, "%s", line);
            a->final = f;
            if (f == AT_FINAL_ERROR) {
                a->errors++;
                return AT_E_ERROR;
            }
            return AT_OK;
        }
    }

    if (a->kind == AT_CHANNEL_TRANSCRIPT) {
        char marker = 0;
        char line[AT_LINE_MAX];
        for (;;) {
            at_status s = script_line(a, &marker, line, sizeof line);
            if (s != AT_OK)
                return s;
            if (marker == '!') {
                if (strcmp(line, "timeout") == 0) {
                    if (a->log != NULL)
                        fprintf(a->log, "! timeout\n");
                    a->timeouts++;
                    return AT_E_TIMEOUT;
                }
                return AT_E_FORMAT;
            }
            if (marker != '<') {
                /* The transcript moved on to the next command while this one
                 * was still waiting for its final line. */
                return AT_E_FORMAT;
            }
            log_line(a, '<', line);
            at_final f = classify(line);
            if (f == AT_FINAL_NONE) {
                keep(a, line);
                continue;
            }
            snprintf(a->final_line, sizeof a->final_line, "%s", line);
            a->final = f;
            if (f == AT_FINAL_ERROR) {
                a->errors++;
                return AT_E_ERROR;
            }
            return AT_OK;
        }
    }
    return AT_E_ARG;
}

static at_status send_raw(atchat *a, const char *text, bool add_cr)
{
    if (a->kind == AT_CHANNEL_SERIAL) {
        size_t n = strlen(text);
        if (n > 0 && write(a->fd, text, n) != (ssize_t)n)
            return AT_E_IO;
        if (add_cr && write(a->fd, "\r", 1) != 1)
            return AT_E_IO;
        return AT_OK;
    }
    if (a->kind == AT_CHANNEL_TRANSCRIPT) {
        char marker = 0;
        char expected[AT_LINE_MAX];
        at_status s = script_line(a, &marker, expected, sizeof expected);
        if (s != AT_OK)
            return s;
        if (marker != '>')
            return AT_E_FORMAT;
        if (strcmp(expected, text) != 0)
            return AT_E_MISMATCH;
        return AT_OK;
    }
    return AT_E_ARG;
}

at_status atchat_command_timeout(atchat *a, const char *cmd, unsigned timeout_ms)
{
    if (a == NULL || cmd == NULL || a->kind == AT_CHANNEL_NONE)
        return AT_E_ARG;
    a->commands++;
    log_line(a, '>', cmd);
    at_status s = send_raw(a, cmd, true);
    if (s != AT_OK)
        return s;
    return read_until_final(a, timeout_ms);
}

at_status atchat_command(atchat *a, const char *cmd)
{
    return atchat_command_timeout(a, cmd, a == NULL ? 0 : a->timeout_ms);
}

at_status atchat_payload(atchat *a, const char *payload)
{
    if (a == NULL || payload == NULL || a->kind == AT_CHANNEL_NONE)
        return AT_E_ARG;
    if (a->final != AT_FINAL_PROMPT)
        return AT_E_ARG;   /* no prompt is outstanding, so nothing wants this */

    log_line(a, '>', payload);
    at_status s = send_raw(a, payload, false);
    if (s != AT_OK)
        return s;
    if (a->kind == AT_CHANNEL_SERIAL) {
        const char ctrl_z = 0x1A;
        if (write(a->fd, &ctrl_z, 1) != 1)
            return AT_E_IO;
    }
    return read_until_final(a, a->timeout_ms);
}

const char *atchat_response(const atchat *a)
{
    return (a == NULL) ? "" : a->resp;
}

const char *atchat_find(const atchat *a, const char *prefix)
{
    if (a == NULL || prefix == NULL)
        return NULL;
    size_t n = strlen(prefix);
    const char *p = a->resp;
    while (*p != '\0') {
        if (strncmp(p, prefix, n) == 0)
            return p;
        const char *nl = strchr(p, '\n');
        if (nl == NULL)
            break;
        p = nl + 1;
    }
    return NULL;
}

bool at_field(const char *line, unsigned n, char *out, size_t cap)
{
    if (line == NULL || out == NULL || cap == 0)
        return false;
    out[0] = '\0';

    const char *p = strchr(line, ':');
    p = (p == NULL) ? line : p + 1;

    for (unsigned i = 0; i < n; i++) {
        const char *comma = strchr(p, ',');
        const char *nl = strchr(p, '\n');
        if (comma == NULL || (nl != NULL && comma > nl))
            return false;
        p = comma + 1;
    }
    while (*p == ' ')
        p++;

    size_t len = 0;
    while (p[len] != '\0' && p[len] != ',' && p[len] != '\n' && p[len] != '\r')
        len++;
    while (len > 0 && p[len - 1] == ' ')
        len--;
    if (len == 0)
        return false;
    if (len >= cap)
        len = cap - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return true;
}
