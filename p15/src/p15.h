/* P15, the semaphore: three indicator modules driven from three host pins.
 *
 * The chapter's fourth acceptance item asks for "a blink program [that] runs
 * three modules as a semaphore from the host's own pins, and all three are
 * seen lit at once". Three `pinctrl` commands have stood in for that so far,
 * and they light three LEDs, so it is worth saying at the top what a program
 * does that they do not:
 *
 *   - It owns the lines. `pinctrl` writes the pad registers under whatever
 *     else holds them; a line request asks the kernel, and is refused when
 *     another process has them. On this bench that other process exists:
 *     bench-status drives indicators through libgpiod on the same chip.
 *   - It releases them. The chapter asks the program to restore the pins on
 *     the way out, and with the character device that is not the program's
 *     promise to keep: the lines are released when the descriptor closes,
 *     which happens when the process dies for any reason, including a kill
 *     no exit handler survives. `pinctrl` leaves the pin driven after it
 *     exits, which the chapter says plainly, and which is why settling the
 *     polarity by hand takes a third command to release the line.
 *   - It changes three lines in one call, so "all three lit at once" is one
 *     instant rather than three.
 *
 * The decision and the device are kept apart, as everywhere else in this
 * volume. Which channels are lit at a given moment is arithmetic on a time,
 * and the suite settles all of it with no chip, no board and no clock. What
 * reaches a pin is a bitmask, and only lines.c knows there is a kernel.
 *
 * The three rules of the volume hold: nothing allocates on the data path,
 * library code returns a status and never exits, and nothing is repaired.
 */
#ifndef P15_H
#define P15_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------- channels */

/* The meanings are not this lab's invention. P01, P02 and P03 each drive an
 * indicator, and their chapters agree on what the colours say, so this lab
 * carries the same three rather than numbering them. */
#define P15_CHANNELS 3u

enum {
    P15_GREEN = 0,   /* registered */
    P15_YELLOW = 1,  /* a default route */
    P15_RED = 2      /* an event */
};

/* A lit set is a bitmask over the three channels above. Three bits, so the
 * eight states of this semaphore fit in one byte and a test can walk all of
 * them rather than sampling. */
#define P15_LIT_NONE 0x00u
#define P15_LIT_ALL  0x07u

#define P15_LIT(ch) ((uint8_t)(1u << (unsigned)(ch)))

/* ----------------------------------------------------------------- wiring
 *
 * Measured on Friday 2 October 2026 on a Raspberry Pi 3 Model B Rev 1.2:
 * header pins 11, 13 and 15, which are BCM 17, 27 and 22.
 *
 * The offset below is the BCM number because on this chip the character
 * device's line offset happens to equal it. That is a property of
 * pinctrl-bcm2835 and not a general truth, which is why p15_chip_matches()
 * exists and why nothing here drives a line before the chip has given its
 * label. Driving offsets 17, 27 and 22 on a chip that numbers its lines
 * differently is three arbitrary pins, and a semaphore wired to a dated
 * observation of one board is only a semaphore on that board.
 */
typedef struct {
    unsigned offset;        /* the character device's line offset on gpiochip0 */
    unsigned header_pin;    /* the physical pin, which is what a reader counts */
    const char *colour;
    const char *meaning;
} p15_wire;

const p15_wire *p15_wiring(void);               /* P15_CHANNELS entries */
const p15_wire *p15_wire_of(unsigned channel);  /* NULL out of range */

/* The label gpiodetect reported on this bench, against which the wiring
 * above was recorded. */
#define P15_CHIP_LABEL "pinctrl-bcm2835"
#define P15_CHIP_PATH  "/dev/gpiochip0"

bool p15_chip_matches(const char *label);

/* ---------------------------------------------------------------- polarity
 *
 * Settled on Friday 2 October 2026 by the chapter's three controls, in the
 * order high, low, released: lit, dark, dark. The header pin sources the
 * current and the module returns it to the common ground rail, so high is
 * the lit state.
 *
 * The other arrangement is carried here rather than assumed away, because
 * the chapter is explicit that a module that sinks inverts every polarity in
 * it, and because which way round the LED sits is not printed on the board.
 * A reader whose module is dark on high has the other kind, and should say
 * so on the command line rather than reading an inverted bench.
 */
typedef enum {
    P15_SOURCING = 0,   /* high is lit; measured on this bench */
    P15_SINKING = 1     /* low is lit; not this bench's modules */
} p15_polarity;

/* The line values that put exactly `lit` on the bench. Only the low three
 * bits are ever set. */
uint8_t p15_bits_for(uint8_t lit, p15_polarity polarity);

/* And back, so the suite can state the round trip rather than restating the
 * inversion a second time and agreeing with itself. */
uint8_t p15_lit_from(uint8_t bits, p15_polarity polarity);

/* The dark pattern under a polarity. The line request carries this as its
 * initial output value, so the lines are never briefly driven to a state
 * nobody chose: with the character device the first value is part of the
 * request rather than a write that follows it. */
uint8_t p15_dark_bits(p15_polarity polarity);

/* ------------------------------------------------------------------- plan
 *
 * The blink the acceptance item asks for, as a function of a time in
 * milliseconds. The clock is an argument here for the same reason it is one
 * in P13: the suite tests the boundaries exactly, at the millisecond either
 * side, instead of sleeping through the sequence and hoping.
 *
 * The order matters and is the point of the sequence. Each channel is lit
 * alone before the three are lit together, because a reader who cannot name
 * which wire is which learns nothing from three lit LEDs. Three lit at once
 * is the acceptance item; three lit one at a time first is what makes it an
 * observation about three channels rather than about three LEDs.
 */
#define P15_STEP_MS 600u
#define P15_STEPS   5u
#define P15_PLAN_MS (P15_STEP_MS * P15_STEPS)

/* The lit set at `ms` into a cycle. Times at or past one cycle wrap, so a
 * run of any length is this function and a sleep. */
uint8_t p15_frame_at(uint32_t ms);

/* The step index at `ms`, for printing the plan as a table. */
unsigned p15_step_at(uint32_t ms);

/* The lit set of step `i`, so the plan can be printed with no clock at all.
 * Out of range returns P15_LIT_NONE, which is also step 4, so a caller that
 * needs to tell them apart should bound the index itself. */
uint8_t p15_step_lit(unsigned i);

/* ----------------------------------------------------------------- budget
 *
 * Measured on Saturday 3 October 2026, each module alone on a source meter,
 * window on the lit stretch. Microamps rather than milliamps and floats: the
 * table has three significant figures, integers carry them exactly, and
 * nothing in this program needs to divide.
 *
 * Four modules were measured and three carry meaning. The chapter quotes a
 * total for one triple and attaches it to the other, which is why these are
 * two separate tables here rather than one sum.
 */
typedef struct {
    const char *colour;
    uint32_t ua_at_5v0;
    uint32_t ua_at_3v3;
} p15_module;

#define P15_MODULES 4u

const p15_module *p15_modules(void);                       /* P15_MODULES entries */
const p15_module *p15_module_of(const char *colour);       /* NULL if not measured */

/* The draw of a lit set, at 3.3 V, in microamps. An unlit channel draws
 * nothing, so the dark semaphore is zero and not a floor. */
uint32_t p15_draw_ua(uint8_t lit);

/* The largest single channel in a lit set, which is the figure a per-pin
 * limit is compared against. The sum is the figure a whole-header limit is
 * compared against. Two different numbers against two different limits, and
 * this volume prints no figure for either, so nothing here compares them to
 * anything. */
uint32_t p15_peak_channel_ua(uint8_t lit);

/* Microamps as milliamps to one decimal, which is the precision the source
 * meter gave. Integer arithmetic throughout: this program has no floating
 * point anywhere, so there is no rounding to argue about and the strict
 * build has nothing to say about promotions.
 *
 * It is a named function rather than two divisions at the call site because
 * the carry is a real case. A remainder that rounds up to ten has to carry
 * into the whole part, and a formatter that cannot do that prints "19.10"
 * on the day the measurement lands there. Needs 16 bytes. */
bool p15_format_ma(char *buf, size_t n, uint32_t ua);

/* ------------------------------------------------------------------ lines
 *
 * The device half. Everything above this line runs anywhere; everything
 * below needs Linux and a chip, and says so rather than returning plausible
 * numbers from a stub.
 */
typedef enum {
    P15_OK = 0,
    P15_E_ARG,        /* a caller's mistake */
    P15_E_NO_CHIP,    /* the chip would not open at all */
    P15_E_CHIP_INFO,  /* it opened and would not say what it is */
    P15_E_WRONG_CHIP, /* it said, and it is not the one the wiring is from */
    P15_E_BUSY,       /* a line is held by something else, which is named */
    P15_E_REQUEST,    /* the request failed for another reason */
    P15_E_SET,        /* the values did not reach the lines */
    P15_E_PLATFORM    /* built for something that is not Linux */
} p15_status;

const char *p15_strstatus(p15_status s);

/* What holds each of the three lines, read before asking for them. The
 * consumer name is why: EBUSY on its own sends a reader to the wiring, and
 * the name sends them to the process. */
typedef struct {
    bool used[P15_CHANNELS];
    char consumer[P15_CHANNELS][32];
    char chip_label[32];
    unsigned chip_lines;
} p15_report;

p15_status p15_probe(const char *chip_path, p15_report *out);

typedef struct {
    int chip_fd;
    int line_fd;
    p15_polarity polarity;
    uint8_t last_bits;
} p15_lines;

/* Requests the three lines of the wiring table, dark, with this program's
 * name as the consumer. Refuses a chip whose label is not the one the wiring
 * was recorded against unless `any_chip` says the caller knows better.
 *
 * There is deliberately no signal handler anywhere in this program. The
 * lines are released when line_fd closes, and the kernel closes it when the
 * process ends however it ends. A handler would add a path that can fail to
 * a guarantee that currently cannot.
 */
p15_status p15_lines_open(p15_lines *l, const char *chip_path, bool any_chip,
                          p15_polarity polarity);

/* `lit` is a lit set, not line values: the polarity given at open decides
 * what reaches the pins, so no caller has to remember it twice. */
p15_status p15_lines_set(p15_lines *l, uint8_t lit);

void p15_lines_close(p15_lines *l);

#endif /* P15_H */
