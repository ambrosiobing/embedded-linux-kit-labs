/* P02, the host half of the NB-IoT power-saving field node.
 *
 * The lab is the duty cycle, not the payload. Two timers are requested with
 * AT+CPSMS as eight-character bitmaps, the network grants what it grants, and
 * the whole question of the lab is whether the radio is asleep between sends.
 *
 * So the program's first job is arithmetic nobody does by hand: turning those
 * bitmaps into seconds. A host script that writes "00100011" into a command
 * and never decodes it cannot tell a three hour update interval from a three
 * minute one, and both look equally plausible on the glass.
 *
 * Its second job is to say what the readback means and does not mean. A
 * module that returns the octets it was given has told you what it asked
 * for, not what it was granted. The grant arrives from the network, and the
 * chapter's fallback exists because it often does not arrive at all.
 *
 * The three rules of this volume hold here too: nothing allocates on the data
 * path, library code returns a status and never exits, and nothing is
 * repaired. A command that times out is a timeout, not an empty reply.
 *
 * One rule of its own, from the chapter: the indicator follows the
 * registration state and nothing else. Not the signal strength, not whether
 * the last datagram was accepted. On a radio that is asleep most of the time
 * neither of those means anything between wakes, and an indicator that mixes
 * them in is an indicator that lies twice an hour.
 */
#ifndef P02_H
#define P02_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../../lib/atchat.h"

#define P02_BITS 8                   /* an AT+CPSMS timer is eight characters */
#define P02_PAYLOAD_MAX_BYTES 20     /* the chapter's budget, as a default */
#define P02_PERIOD_DEFAULT_S 600
#define P02_CYCLES_DEFAULT 3

typedef enum {
    P02_OK = 0,
    P02_E_IO,
    P02_E_FORMAT,
    P02_E_RANGE,
    P02_E_ARG,
    P02_E_AT,        /* the module refused, or said nothing */
    P02_E_PLAN       /* the schedule and the timers contradict each other */
} p02_status;

const char *p02_strerror(p02_status s);

/* ---------------------------------------------------------------- timers */

/* The two timers of AT+CPSMS are different encodings with the same shape:
 * three bits of unit and five bits of value. They are defined in 3GPP TS
 * 24.008, the periodic update timer as GPRS Timer 3 in 10.5.7.4a and the
 * active time as GPRS Timer in 10.5.7.3, and the two unit tables are not the
 * same table. Reading one with the other's units is the mistake this type
 * exists to prevent, which is why the kind is an argument and not a guess. */
typedef enum {
    P02_TIMER_TAU = 0,     /* T3412 extended, GPRS Timer 3, 10.5.7.4a */
    P02_TIMER_ACTIVE       /* T3324, GPRS Timer, 10.5.7.3 */
} p02_timer_kind;

typedef struct {
    bool deactivated;      /* the unit field says the timer is off */
    uint32_t seconds;      /* zero when deactivated */
    uint8_t unit;          /* the three bits, as read */
    uint8_t value;         /* the five bits, as read */
    char bits[P02_BITS + 1];
} p02_timer;

/* Eight characters of '0' and '1', most significant first. Anything else is
 * refused rather than read as far as it goes. */
p02_status p02_timer_parse(p02_timer *t, p02_timer_kind kind, const char *bits);
void p02_timer_print(const p02_timer *t, p02_timer_kind kind, FILE *out);

/* ---------------------------------------------------------------- the plan */

/* What the two timers and the send period add up to. The acceptance test
 * counts datagrams at the far end, and this is the same count worked out in
 * advance, which is what makes the measurement a check rather than a
 * discovery. */
typedef struct {
    p02_timer tau;
    p02_timer active;
    uint32_t period_s;        /* how often the node sends */
    double sends_per_hour;
    double awake_seconds_per_hour;
    double awake_fraction;
    bool radio_ever_sleeps;   /* false when the period is inside the active window */
    bool tau_outlives_period; /* the update interval is longer than the send period */
} p02_plan;

p02_status p02_plan_make(p02_plan *p, const p02_timer *tau,
                         const p02_timer *active, uint32_t period_s);
void p02_plan_print(const p02_plan *p, FILE *out);

/* ---------------------------------------------------------------- payload */

/* AT+CSOSEND carries hexadecimal, so the wire length is half the string.
 * Odd length, a character that is not a hexadecimal digit, or more bytes than
 * the budget are all refused: a field node whose record grew past its plan is
 * the failure this lab is written against, and it is cheaper to catch here
 * than in a data bill. */
p02_status p02_payload_check(const char *hex, uint32_t max_bytes, uint32_t *bytes);

/* ---------------------------------------------------------------- session */

typedef enum {
    P02_REG_UNKNOWN = 0,
    P02_REG_NOT_REGISTERED,
    P02_REG_SEARCHING,
    P02_REG_DENIED,
    P02_REG_HOME,
    P02_REG_ROAMING
} p02_reg;

const char *p02_reg_name(p02_reg r);
/* The second field of +CEREG, which is the state. The first is the
 * unsolicited-result setting and says nothing about the network. */
p02_reg p02_reg_from_cereg(const char *line);

typedef struct {
    const char *endpoint;     /* the CoAP address, as the module will see it */
    uint16_t port;
    const char *payload_hex;
    uint32_t max_payload_bytes;
    uint32_t cycles;
    uint32_t period_s;
    const char *tau_bits;
    const char *active_bits;
    uint8_t band;
} p02_config;

void p02_config_init(p02_config *c);

typedef struct {
    uint32_t cycles;
    uint32_t sends_attempted;
    uint32_t sends_accepted;
    uint32_t led_on_cycles;      /* decided by the registration state alone */
    uint32_t not_registered;
    uint32_t at_timeouts;
    uint32_t at_errors;
    p02_reg last_reg;
    int last_rssi;               /* as +CSQ reports it, 99 meaning unknown */
    bool psm_requested;
    bool psm_readback_matches;   /* the module returned the octets it was given */
    p02_timer tau_requested;
    p02_timer active_requested;
    p02_timer tau_readback;
    p02_timer active_readback;
    bool readback_parsed;
} p02_session;

void p02_session_init(p02_session *s);

/* Bring the module up, request the timers, read them back. */
p02_status p02_bringup(atchat *a, const p02_config *c, p02_session *s);
/* One wake: registration, signal, socket, send. The indicator state for the
 * cycle is decided before the socket is opened and is not revisited. */
p02_status p02_cycle(atchat *a, const p02_config *c, p02_session *s);
void p02_session_print(const p02_session *s, FILE *out);

/* ---------------------------------------------------------------- scenes */

typedef enum {
    P02_SCENE_ATTACHED = 0,   /* registered, power saving requested and echoed */
    P02_SCENE_PSM_REFUSED,    /* registered, the module refuses AT+CPSMS */
    P02_SCENE_SEARCHING,      /* never registers, so nothing is ever sent */
    P02_SCENE_SILENT          /* the module says nothing at all */
} p02_scene;

p02_status p02_scene_write(p02_scene scene, const p02_config *c, const char *path);

#endif /* P02_H */
