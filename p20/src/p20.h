/* P20, the fleet sweep: not the sweep, the thing that judges what it wrote.
 *
 * This lab's deliverable is a shell script of about forty lines and a text
 * file it appends to. Replacing that script with a C program would be a
 * worse answer than the chapter's, and this program does not try: it never
 * opens an SSH session, never asks a host anything, and cannot repair
 * anything. It reads `/var/log/lab-fleet.txt` after the fact and decides
 * whether that file is the acceptance artifact the chapter says it is.
 *
 * The chapter is explicit that the file is the product: there is no
 * dashboard, no database and no alerting, and when the acceptance test is
 * run it is run against that file. So the file is what gets a program.
 *
 * Four of the chapter's seven pitfalls are decidable from the file alone and
 * they are what this judges:
 *
 *   a blank where a HAT should be named   report `none` explicitly, because
 *                                         a blank reads as a broken script
 *                                         rather than as an empty header
 *   a fifth name in the loop              the fleet is four hosts, and a
 *                                         fifth is a host this book has not
 *                                         brought up
 *   two HATs on one host                  the volume's second rule, which
 *                                         every other lab obeys
 *   an undated sweep                      a file that does not say when it
 *                                         was taken cannot be compared with
 *                                         a bench a person can see
 *
 * **The one piece of judgement is which absences are honest.** `vcgencmd` is
 * a Raspberry Pi tool and the NanoPi has none, and the header bus there is 0
 * rather than 1, as P09 established. The chapter's sweep discards the error
 * output so that both show up as a shorter block, and says that a shorter
 * block is a true statement about that host. In the file those absences look
 * exactly like a command that failed on a Pi. This program is what knows the
 * difference: the same missing line is an honest absence under one host and
 * a gap under another, and it is counted as one or the other rather than
 * averaged into a single number nobody can act on.
 *
 * Nothing is repaired, nothing is inferred, and a line this program does not
 * recognise is counted rather than guessed at.
 */
#ifndef P20_H
#define P20_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define P20_HOSTS    4u
#define P20_NAME_MAX 32u

/* The fleet, in the order the chapter's loop names them. A fifth entry is
 * not a configuration option: the chapter's own pitfall says a fifth name is
 * a host this book has not brought up. */
typedef struct {
    const char *name;
    bool answers_temp;    /* vcgencmd is a Raspberry Pi tool */
    unsigned header_bus;  /* 1 on a Pi, 0 on the NanoPi, from P09 */
    const char *what;
} p20_host;

const p20_host *p20_fleet(void);                 /* P20_HOSTS entries */
const p20_host *p20_host_of(const char *name);   /* NULL for a stranger */

/* ------------------------------------------------------------------ sweep */

typedef struct {
    bool seen;
    bool hat_reported;               /* a labelled hat line appeared at all */
    bool hat_named;                  /* and it named a board rather than none */
    char hat[P20_NAME_MAX];
    unsigned hats_named;              /* two is the exclusivity violation */
    bool temp_seen;
    bool ram_seen;
} p20_block;

typedef struct {
    p20_block b[P20_HOSTS];
    bool dated;
    char stranger[P20_NAME_MAX];     /* the first host name not in the fleet */
    unsigned strangers;
    uint32_t lines;
    uint32_t before_any_host;        /* lines that belong to no block */
} p20_sweep;

typedef enum {
    P20_OK = 0,
    P20_E_ARG,
    P20_E_OPEN,
    P20_E_EMPTY
} p20_status;

const char *p20_strstatus(p20_status s);

p20_status p20_read(const char *path, p20_sweep *out);

/* ---------------------------------------------------------------- verdict */

typedef struct {
    unsigned present;                /* how many of the four appeared */
    bool four_hosts;
    bool every_hat_reported;
    unsigned first_silent_hat;       /* a host index, when one was silent */
    bool exclusive;
    unsigned doubled;                /* the host that named two */
    bool no_stranger;
    bool dated;

    /* The two kinds of absence, kept apart on purpose. */
    unsigned honest_absences;        /* a field that never applied to that host */
    unsigned gaps;                   /* a field that should have answered */
    unsigned first_gap_host;
} p20_verdict;

void p20_judge(const p20_sweep *s, p20_verdict *v);
bool p20_verdict_passed(const p20_verdict *v);

/* ----------------------------------------------------------------- scenes */

typedef enum {
    P20_SCENE_GOOD = 0,     /* four hosts, every HAT reported, one seated */
    P20_SCENE_BLANK_HAT,    /* a host whose HAT line never appeared */
    P20_SCENE_FIFTH,        /* a fifth name in the loop */
    P20_SCENE_TWO_HATS,     /* two HATs on one host */
    P20_SCENE_UNDATED,      /* no timestamp, so nothing can be compared */
    P20_SCENE_PI_NO_TEMP    /* a Pi that did not answer vcgencmd */
} p20_scene_kind;

bool p20_scene_of(const char *name, p20_scene_kind *out);
p20_status p20_scene_write(p20_scene_kind kind, FILE *out);

#endif /* P20_H */
