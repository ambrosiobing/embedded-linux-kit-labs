/* P03, the host half of the Cat-M node that stamps its payload from GNSS.
 *
 * The sequence is the product of this lab. Lock the bearer, confirm it,
 * power the receiver, poll until the fix is usable, power the receiver
 * down, then open the session and publish. The host never holds both
 * subsystems active and never blocks the publish on a cold start.
 *
 * So the program is built around an ordering rather than a number, and the
 * ordering is enforced rather than described. A transmit attempted while the
 * GNSS engine is powered is refused by the function that would have sent it,
 * and the refusal stops the cycle. That is the difference between a chapter
 * that says "always" and a program in which it is always true.
 *
 * Three rules of the volume, and two of this lab's own.
 *
 * Nothing allocates on the data path. Library code returns a status and
 * never exits. Nothing is repaired.
 *
 * The stamp in the payload is the receiver's and never the host's clock. A
 * cycle with no fix and no cached fix is skipped, not filled in. Substituting
 * the Pi's clock would make a payload that looks right and means something
 * else, and it would destroy the one property that makes a repeat visible.
 *
 * A repeated stamp is a repeat, and is published as one. When a cycle has no
 * new fix, the cached one goes out again with the stamp it came with, so a
 * receiver sees the same stamp twice and knows. The moment a host clock is
 * substituted that property is gone, which is the whole argument of this
 * lab's rule.
 */
#ifndef P03_H
#define P03_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../../lib/atchat.h"
#include "../../lib/linereader.h"

#define P03_STAMP_MAX 32
#define P03_COORD_MAX 24
#define P03_CYCLES_DEFAULT 3

typedef enum {
    P03_OK = 0,
    P03_E_IO,
    P03_E_FORMAT,
    P03_E_ARG,
    P03_E_AT,       /* the module refused, or said nothing */
    P03_E_BEARER,   /* the radio is not on Cat-M */
    P03_E_ORDER     /* a transmit was attempted with the receiver powered */
} p03_status;

const char *p03_strerror(p03_status s);

/* ---------------------------------------------------------------- the fix */

typedef struct {
    bool valid;
    bool cached;                   /* published again, with its own stamp */
    char stamp[P03_STAMP_MAX];     /* as the receiver reported it */
    char lat[P03_COORD_MAX];
    char lon[P03_COORD_MAX];
} p03_fix;

void p03_fix_clear(p03_fix *f);

/* +CGNSINF: <run>,<fix>,<stamp>,<lat>,<lon>,...
 *
 * A fix is usable only when the fix field is 1 and the stamp and both
 * coordinates are present. A line with a run status and nothing else is the
 * normal answer from a receiver that has been powered for four seconds, and
 * it is not an error. */
bool p03_fix_parse(const char *cgnsinf_line, p03_fix *f);

/* The payload: the receiver's stamp, then the two coordinates. Returns the
 * number of characters, or zero when the fix is not usable. */
size_t p03_payload(const p03_fix *f, char *out, size_t cap);

/* ---------------------------------------------------------------- bearer */

/* +CPSI: LTE CAT-M1,Online,... is what this lab requires. NB-IoT and a GPRS
 * fallback both answer the same AT commands and both publish happily, which
 * is why the bearer is read rather than assumed. */
bool p03_bearer_is_catm(const char *cpsi_line);

/* ---------------------------------------------------------------- session */

typedef struct {
    const char *broker;
    uint16_t port;
    const char *client_id;
    const char *topic;
    uint32_t cycles;
} p03_config;

void p03_config_init(p03_config *c);

typedef struct {
    uint32_t cycles;
    uint32_t fixes_new;
    uint32_t fixes_cached;     /* published again, marked as such */
    uint32_t skipped;          /* no fix and nothing cached: nothing published */
    uint32_t published;
    uint32_t order_refusals;   /* a transmit attempted with the receiver on */
    uint32_t at_errors;
    uint32_t at_timeouts;
    bool bearer_catm;          /* what +CPSI last said */
    bool indicator;            /* the bearer is Cat-M and the session is open */
    bool gnss_powered;         /* the program's own model of the receiver */
    p03_fix last_fix;
    char last_payload[128];
} p03_session;

void p03_session_init(p03_session *s);

p03_status p03_bringup(atchat *a, const p03_config *c, p03_session *s);
p03_status p03_cycle(atchat *a, const p03_config *c, p03_session *s);
void p03_session_print(const p03_session *s, FILE *out);

/* The only way anything in this program transmits, and the place the rule
 * lives. With the receiver powered it returns P03_E_ORDER and sends nothing
 * at all, so the module never sees the command.
 *
 * It is declared here rather than kept private for one reason: as the cycle
 * is written today this guard cannot be reached, because every path that
 * reaches a transmit has powered the receiver down first. A guard that
 * cannot be reached is a guard nothing tests, and an untested guard quietly
 * stops working. The suite calls it directly, with the receiver marked
 * powered, and checks that the command was not sent. */
p03_status p03_transmit(atchat *a, p03_session *s, const char *cmd);

/* ---------------------------------------------------------------- the audit */

/* The same ordering, read off a recorded session rather than enforced during
 * one. This is what to run over a transcript brought back from the bench,
 * including one recorded by a different program or typed by hand in minicom.
 */
typedef struct {
    uint32_t gnss_on;          /* how many times the receiver was powered */
    uint32_t gnss_off;
    uint32_t transmits;
    uint32_t transmits_with_gnss_on;
    uint32_t first_bad_line;   /* where the first one was, 1 based */
    /* Big enough for any line the reader can hand over, so that the command
     * this names cannot be a cut-down version of the command that was sent.
     * It was AT_LINE_MAX, which is smaller than a line, and the compiler
     * asked about it: a buffer that holds a line should be able to hold a
     * line, and the two sizes should not be able to drift apart. */
    char first_bad_cmd[LINEREADER_MAX + 1];
    uint32_t lines;
} p03_audit;

p03_status p03_audit_file(const char *path, p03_audit *out);
bool p03_audit_clean(const p03_audit *a);

/* ---------------------------------------------------------------- scenes */

typedef enum {
    P03_SCENE_FIX = 0,        /* a fix on every cycle */
    P03_SCENE_CACHED,         /* a fix on the first cycle only */
    P03_SCENE_NO_FIX,         /* never a fix, so nothing is ever published */
    P03_SCENE_WRONG_BEARER,   /* registered, but not on Cat-M */
    P03_SCENE_GNSS_LEFT_ON    /* a transcript that breaks the lab's own rule */
} p03_scene;

p03_status p03_scene_write(p03_scene scene, const p03_config *c, const char *path);

#endif /* P03_H */
