/* The state machine, the tag reader and the log.
 *
 * Nothing in this file reads a clock. The time arrives as an argument, from
 * a monotonic clock during a run and from a column of a log during a
 * replay, and the machine cannot tell the difference. That is what lets the
 * suite drive the whole of this lab's behaviour with no timing in it.
 */

#include "p13.h"

#include <string.h>

const char *p13_strerror(p13_status s)
{
    switch (s) {
    case P13_OK:       return "ok";
    case P13_E_IO:     return "the file could not be read or written";
    case P13_E_FORMAT: return "the content is not in the expected form";
    case P13_E_ARG:    return "an argument is missing or will not do";
    }
    return "unknown";
}

const char *p13_state_name(p13_state s)
{
    return (s == P13_PRIMARY) ? "PRIMARY" : "FAILOVER";
}

bool p13_tag_of(const char *line, char *tag, size_t cap)
{
    if (line == NULL || tag == NULL || cap == 0)
        return false;
    tag[0] = '\0';

    const char *p = strstr(line, "\"src\"");
    if (p == NULL)
        return false;
    p = strchr(p + 5, ':');
    if (p == NULL)
        return false;
    p++;
    while (*p == ' ')
        p++;
    if (*p != '"')
        return false;
    p++;

    size_t n = 0;
    while (p[n] != '\0' && p[n] != '"' && n + 1 < cap)
        n++;
    if (p[n] != '"' || n == 0)
        return false;
    memcpy(tag, p, n);
    tag[n] = '\0';
    return true;
}

void p13_init(p13_supervisor *s, double window_s)
{
    if (s == NULL)
        return;
    memset(s, 0, sizeof *s);
    s->window_s = (window_s > 0.0) ? window_s : P13_WINDOW_DEFAULT_S;
    /* Nothing has been heard yet. A supervisor that starts in PRIMARY is
     * claiming a route it has never seen. */
    s->state = P13_FAILOVER;
    s->first_failover_at = -1.0;
    s->last_transition_at = -1.0;
}

p13_event p13_on_record(p13_supervisor *s, double now,
                        const char *tag, const char *payload)
{
    if (s == NULL || tag == NULL)
        return P13_EVENT_NONE;

    if (strcmp(tag, P13_PRIMARY_TAG) == 0) {
        s->primary_records++;
        s->last_primary_at = now;
        s->heard_primary = true;
        if (s->state != P13_PRIMARY) {
            s->state = P13_PRIMARY;
            s->to_primary++;
            s->last_transition_at = now;
            return P13_EVENT_TO_PRIMARY;
        }
        return P13_EVENT_NONE;
    }

    if (strcmp(tag, P13_BACKUP_TAG) == 0) {
        s->backup_records++;
        s->have_backup = true;
        if (payload != NULL)
            snprintf(s->backup, sizeof s->backup, "%s", payload);
        /* The backup never changes the state. It is the reading that becomes
         * live when the primary is late, and a route is not declared good
         * because a different route spoke. */
        return P13_EVENT_NONE;
    }

    s->unreadable++;
    return P13_EVENT_NONE;
}

p13_event p13_on_tick(p13_supervisor *s, double now)
{
    if (s == NULL)
        return P13_EVENT_NONE;
    if (s->state != P13_PRIMARY)
        return P13_EVENT_NONE;
    if (!s->heard_primary)
        return P13_EVENT_NONE;
    if (now - s->last_primary_at <= s->window_s)
        return P13_EVENT_NONE;

    s->state = P13_FAILOVER;
    s->to_failover++;
    s->last_transition_at = now;
    if (s->first_failover_at < 0.0)
        s->first_failover_at = now;
    return P13_EVENT_TO_FAILOVER;
}

/* ---------------------------------------------------------------- the log */

p13_status p13_log_open(p13_log *l, const char *path)
{
    if (l == NULL || path == NULL)
        return P13_E_ARG;
    l->fp = fopen(path, "wb");
    if (l->fp == NULL)
        return P13_E_IO;
    fprintf(l->fp, "# p13 log, seconds from the start of the run\n");
    return P13_OK;
}

void p13_log_record(p13_log *l, double t, const char *tag)
{
    if (l == NULL || l->fp == NULL || tag == NULL)
        return;
    fprintf(l->fp, "%.3f record %s\n", t, tag);
}

void p13_log_state(p13_log *l, double t, p13_state s, double age)
{
    if (l == NULL || l->fp == NULL)
        return;
    if (s == P13_FAILOVER)
        fprintf(l->fp, "%.3f state FAILOVER age %.3f\n", t, age);
    else
        fprintf(l->fp, "%.3f state PRIMARY\n", t);
}

p13_status p13_log_close(p13_log *l)
{
    if (l == NULL)
        return P13_E_ARG;
    if (l->fp != NULL && fclose(l->fp) != 0) {
        l->fp = NULL;
        return P13_E_IO;
    }
    l->fp = NULL;
    return P13_OK;
}
