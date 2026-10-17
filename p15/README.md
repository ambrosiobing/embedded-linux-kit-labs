# P15: the semaphore of the mixed-voltage discipline lab

The host side of lab P15, in C. Three output lines, five steps and two dated
observations.

**What it does.** Drives three LK-LED10 indicator modules from header pins 11,
13 and 15 as a semaphore, through the GPIO character device. Prints the blink
plan, the measured draw and the wiring table without touching a chip, and
reports what holds the three lines before asking for them.

## Why a program, when three commands already light three LEDs

The chapter's fourth acceptance item asks for a blink program, and three
`pinctrl` commands have stood in for one so far. They do light three LEDs, so
the difference is worth stating:

- **It owns the lines.** `pinctrl` writes the pad registers under whatever
  else holds them. A line request asks the kernel and is refused when another
  process has them, which on this bench is a real case: `bench-status` drives
  indicators through libgpiod on the same chip. `p15 probe` reports the
  holder by name, because `EBUSY` on its own sends a reader to the wiring.
- **It releases them, and not by remembering to.** The chapter asks the
  program to restore the pins on the way out. Here that is the kernel's doing
  rather than this program's promise: the lines go when the descriptor closes,
  which happens however the process ends, including under a signal no handler
  sees. There is deliberately no signal handler in this program. `pinctrl`
  leaves the pin driven after it exits, which is why settling the polarity by
  hand takes a third command to release the line.
- **Three lines change in one call**, so "all three lit at once" is one
  instant rather than three.
- **It refuses the wrong chip.** The offsets are the BCM numbers because on
  `pinctrl-bcm2835` they are the same number. That is a property of this chip,
  so nothing is driven before the chip has given its label.

## State, stated plainly

**Built with warnings as errors on Saturday 17 October 2026, and again under
the stricter conversion pass, which was clean.** What it has not done is
touch hardware: no chip has been opened by it, no line requested, no LED lit.
Its device half has been exercised only by being refused, which is the most a
machine with no header can offer.

The wiring table and the current table are both dated observations from
sittings that did happen, on Friday 2 October 2026 and Saturday 3 October
2026, and they sit in the source with their dates beside them because a
wiring table is an observation and not a constant.

The two numbers this lab still owes its chapter are the rail voltages, and
they need a voltmeter this bench does not have.

## Build and check

    make            build p15 and the suite
    make check      run the suite: no chip, no board, no LEDs, no clock
    make demo       the three subcommands that need no bench, and one refusal
    make strict     a second pass with the conversion warnings turned on

## With no bench

    ./p15 plan        the five steps, the line values and the draw of each
    ./p15 budget      the four measured modules, and which triple is which
    ./p15 wiring      the pins, the offsets and the chip they belong to

`plan` is the one to run before a sitting. The table it prints comes from the
same `p15_frame_at()` that drives the pins, so what the suite proves about the
plan is a statement about what the LEDs will do rather than about a second
description of them.

## On the bench

    ./p15 probe                        what holds the three lines
    ./p15 semaphore --state all        hold three lit, ten seconds
    ./p15 semaphore --state registered green alone
    ./p15 blink --cycles 3             the plan, three times

If the modules sink rather than source, every polarity inverts and
`--sinking` says so. This bench's modules source, settled on Friday 2 October
2026 by the chapter's three controls in the order high, low, released: lit,
dark, dark. The chapter is explicit that this is to be settled by hand before
any program is trusted, and that is still the right order.

## The sequence, and the crossed pair

The plan lights each channel alone before lighting all three. The order is
the point. Three LEDs lit at once show that three pins can source together,
and that is the acceptance item; three LEDs lit one at a time first, in a
known order, show which pin is which.

A reader who has crossed two jumpers sees the all-three step pass. The suite
asserts exactly that: swapping two channels leaves the all-three step
identical and changes two of the other four, so the steps that detect a
crossed pair are named rather than assumed.

## The current figure, and the triple it belongs to

Four modules were measured and three carry meaning. The chapter prints 16.5
mA for "the semaphore" and names the three it measured: blue, green and
yellow. This semaphore's three are green, yellow and red, which is what P01,
P02 and P03 give meanings to, and they draw 19.9 mA at 3.3 V. Both figures
are right about their own triple. `p15 budget` prints both.

The sum belongs beside a whole-header limit and the largest single channel,
green at 7.4 mA, belongs beside a per-pin one. Two different numbers against
two different limits, and this volume names the Raspberry Pi hardware
documentation in the chapter's sources but prints no figure from it, so
nothing here is set against a limit.

## What this program does not do

It does not settle the polarity, which the chapter settles by hand with three
controls and which has been done. It does not measure the rails, which needs
an instrument. It does not run the negative test, which lives on the
breadboard and never touches a header pin. And it does not dismantle
anything, which is the last step of the lab and belongs to the person at the
bench.
