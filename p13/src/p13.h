/* P13, the supervisor of the dual-radio signalling failover.
 *
 * Two states, one timer, one input. The supervisor sees one stream of
 * tagged records and nothing else: it does not see two interfaces, it
 * cannot see a route, and it has no opinion about radios. That is what
 * makes the antenna test meaningful, because when the antenna comes off the
 * only thing that changes in this program's world is that records carrying
 * one tag stop arriving.
 *
 * So the whole of it is a function of a time and a tag, and that is how it
 * is written: nothing in the machine reads a clock. The caller supplies the
 * time, which is a monotonic clock during a live run and a column of a
 * recorded log during a replay. The same state machine produces the same
 * transitions either way, and the suite drives it with no clock at all.
 *
 * Three rules of the volume hold here too, and two of this lab's own.
 *
 * Nothing allocates. Library code returns a status and never exits. Nothing
 * is repaired: a record whose tag cannot be read is counted, not guessed.
 *
 * The tag is read, never assigned. The string cat4 or nb is written by the
 * host that owns the radio, and a tag added here would describe what this
 * program believed rather than what arrived, which is the thing under test.
 *
 * The run starts in FAILOVER. Nothing has been heard yet, and a supervisor
 * that starts in PRIMARY is claiming a route it has never seen.
 */
#ifndef P13_H
#define P13_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../../lib/linereader.h"

#define P13_TAG_MAX 16
#define P13_PAYLOAD_MAX 256
#define P13_WINDOW_DEFAULT_S 45.0
#define P13_PRIMARY_TAG "cat4"
#define P13_BACKUP_TAG "nb"

typedef enum {
    P13_OK = 0,
    P13_E_IO,
    P13_E_FORMAT,
    P13_E_ARG
} p13_status;

const char *p13_strerror(p13_status s);

typedef enum { P13_FAILOVER = 0, P13_PRIMARY } p13_state;
const char *p13_state_name(p13_state s);

typedef enum {
    P13_EVENT_NONE = 0,
    P13_EVENT_TO_PRIMARY,
    P13_EVENT_TO_FAILOVER
} p13_event;

/* ---------------------------------------------------------------- records */

/* A line as mosquitto_sub -v prints it:
 *
 *     lab/p13/health {"src":"cat4","ok":1}
 *
 * The payload is read for one field and nothing else. This is not a JSON
 * parser and does not pretend to be one: the record is fixed by the two
 * publishers of this lab, and a line whose src cannot be found is counted
 * as unreadable rather than interpreted further. */
bool p13_tag_of(const char *line, char *tag, size_t cap);

/* ---------------------------------------------------------------- the machine */

typedef struct {
    double window_s;
    double last_primary_at;     /* in the caller's timebase */
    bool heard_primary;
    char backup[P13_PAYLOAD_MAX];
    bool have_backup;
    p13_state state;

    uint32_t primary_records;
    uint32_t backup_records;
    uint32_t unreadable;
    uint32_t to_failover;
    uint32_t to_primary;
    double first_failover_at;
    double last_transition_at;
} p13_supervisor;

void p13_init(p13_supervisor *s, double window_s);

/* A record arrived at time now. Returns the transition it caused, if any. */
p13_event p13_on_record(p13_supervisor *s, double now,
                        const char *tag, const char *payload);
/* Time passed and nothing arrived. The only thing that can happen here is a
 * fall to FAILOVER, because a route is never declared good by silence. */
p13_event p13_on_tick(p13_supervisor *s, double now);

/* ---------------------------------------------------------------- the log */

/* What a run writes, and what a replay and the acceptance test read:
 *
 *     # p13 log, seconds from the start of the run
 *     0.000 record cat4
 *     0.000 state PRIMARY
 *     60.004 state FAILOVER age 45.004
 *
 * Seconds from the start rather than a wall clock, because the window is a
 * duration and a wall clock can step sideways in the middle of one. */
typedef struct {
    FILE *fp;
} p13_log;

p13_status p13_log_open(p13_log *l, const char *path);
void p13_log_record(p13_log *l, double t, const char *tag);
void p13_log_state(p13_log *l, double t, p13_state s, double age);
p13_status p13_log_close(p13_log *l);

/* Re-run the machine over a recorded log, using the log's own times. */
p13_status p13_replay(const char *path, double window_s, p13_supervisor *out);

/* ---------------------------------------------------------------- the gate */

typedef struct {
    bool both_sources;        /* records from each publisher */
    bool flipped;             /* a fall to FAILOVER happened */
    bool returned;            /* and a return to PRIMARY after it */
    bool flip_in_window;      /* no later than the window after the last primary */
    bool return_on_record;    /* on the first primary record, not on a timer */
    bool backup_rhythm_kept;  /* the backup interval did not change at the flip */
    double worst_flip_lateness;
    double backup_interval_before;
    double backup_interval_after;
    uint32_t primary_records;
    uint32_t backup_records;
} p13_verdict;

p13_status p13_verify(const char *path, double window_s, p13_verdict *v);
bool p13_verdict_passed(const p13_verdict *v);
void p13_verdict_print(const p13_verdict *v, FILE *out);

/* ---------------------------------------------------------------- scenes */

typedef enum {
    P13_SCENE_STEADY = 0,     /* both publishers, no outage */
    P13_SCENE_OUTAGE,         /* the primary stops, then returns */
    P13_SCENE_SHORT_GAP,      /* the primary misses one sample, inside the window */
    P13_SCENE_NO_RETURN,      /* the primary never comes back */
    P13_SCENE_BACKUP_ONLY     /* the primary is never heard at all */
} p13_scene;

typedef struct {
    double minutes;
    double primary_every_s;
    double backup_every_s;
    double outage_start_s;
    double outage_end_s;
} p13_synth;

void p13_synth_init(p13_synth *s);
p13_status p13_scene_write(p13_scene scene, const p13_synth *cfg,
                           double window_s, const char *path);

#endif /* P13_H */
