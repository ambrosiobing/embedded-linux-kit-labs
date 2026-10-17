# P15: what a program does that three commands do not

The lab is [POW-BB mixed-voltage discipline](../chapters/15-pow-bb-mixed-voltage-discipline.md)
and its program was written on **Saturday 17 October 2026**. The lab's other
record, [p15.md](p15.md), is about the two sittings where something was
measured, and it is the only file in this journal describing a bench rather
than a document.

This lab was the one closest to a recorded pass. Three of its seven acceptance
items were already satisfied and written down, and the fourth asked for a
blink program where three `pinctrl` commands had stood in. So the question
this file exists to answer was asked before a line was written: three commands
already light three LEDs, so what is a program for?

## Four answers, and one of them is the real one

**It owns the lines.** `pinctrl` writes the pad registers under whatever else
holds them. A character-device line request asks the kernel, and is refused
while another process owns the line. That is not hypothetical on this bench:
`bench-status` drives indicators through libgpiod on the same chip.

**Three lines change in one call.** The acceptance item says all three are
seen lit at once, and with a line request that is one ioctl rather than three
commands.

**It refuses the wrong chip.** The line offsets are the BCM numbers because on
`pinctrl-bcm2835` they happen to be the same number. On a Pi 5 the chip is
`pinctrl-rp1` and they are not. So the program reads the chip's label before
it drives anything, which makes the dated wiring table from Friday 2 October
2026 refuse to be carried silently to another board.

**And it releases the lines without remembering to**, which is the real one.

The chapter asked the program to set every pin low first and to restore the
pins on the way out. The program does neither, and both are improvements
rather than omissions, which is why the chapter now says so in the same
paragraph:

The initial value is part of the line request, not a write that follows it, so
the lines go from released straight to dark and are never briefly driven to a
state nobody chose. There is no window to get wrong.

And nothing restores them, because the kernel does. The lines are released
when the request's descriptor closes, and the descriptor closes when the
process ends however it ends, including under a signal no handler sees. There
is deliberately no signal handler in this program: a handler would add a path
that can fail to a guarantee that currently has none.

That is the difference worth the typing. `pinctrl` leaves the pin driven after
it exits, which the chapter already said plainly in another context, and which
is why settling a polarity by hand takes a third command just to release the
line. Three commands and a program leave two different benches behind them at
the end of a sitting.

## The ioctls rather than libgpiod, and the reason is about the book

The chapter says a program that holds the lines wants libgpiod v2, which the
host carries at 2.2.1. This program uses `linux/gpio.h` directly instead.

For a command line libgpiod is the better answer and the chapter's `gpioset`
lines stay as they were. The reason to go lower here is not elegance: the
kernel headers are present wherever these programs are compiled, and
`libgpiod-dev` is not. A lab whose program only builds on the board it is
waiting for cannot be checked while it waits, and most of the labs in this
volume are waiting. It is the same argument that put `lib/i2cbus.h` on
i2c-dev rather than on a library, and the same one the suite tests: the
device half has a second implementation for anything that is not Linux, and
CI now reads back which of the two it compiled, because a suite that quietly
built the fallback would pass while proving nothing.

## The step the acceptance item names cannot detect a crossed pair

Three lines driven high are three lines driven high in any order. So the
fourth acceptance item passes with green and yellow swapped, and the swap is
found two labs later in P01 or P02, where green means registered and the bench
shows yellow.

That is why the plan lights each channel alone, in the published order, before
it lights all three, and why the all-three step is fourth rather than first.
The suite asserts both halves rather than leaving them to the reader of a
table: swapping two channels leaves the all-three step identical, and changes
exactly two of the other four. A reader can see from the test output which
steps are the evidence and which step is the one that cannot fail.

This is the same habit as the known positive before a sweep is trusted,
applied to a bench instead of a search. A check that passes whatever is wired
is not a check.

## The figure that belonged to a different three modules

Four modules were measured on Saturday 3 October 2026, each alone on a source
meter. The chapter printed a total for the semaphore:

> The semaphore on the Pi draws 16.5 mA at 3.3 V, blue, green and yellow
> together

Both halves of that sentence are true and they are about different things.
Blue, green and yellow do draw 16.53 mA at 3.3 V. But the semaphore this
chapter defines is green, yellow and red, because those are the three that
carry meanings in P01, P02 and P03, and from the same table they draw 19.93
mA. The difference is 3.4 mA, which is blue swapped for red.

Nothing was mismeasured. A figure was quoted next to the wrong triple, and it
survived because it is a plausible number in the right units with its own
modules named beside it. The arithmetic now lives in `p15 budget`, which adds
both triples up and says which is which, and CI reads both numbers back out of
its output. A sentence can carry a figure from the wrong column. A program
that adds the column up cannot.

While correcting it, a second thing in the same paragraph needed saying. The
old sentence compared 16.5 mA to "what one pin may source", and 16.5 mA is a
total across three pins. The comparison was conservative, so the conclusion
held, but it was the wrong comparison: a per-pin limit is compared against the
largest single channel, green at 7.4 mA, and a whole-header limit against the
sum. Two numbers, two limits. This volume prints no figure for either, so the
chapter now claims no margin at all instead of claiming one against a limit
nobody has quoted from a page.

## What is not here

No `lib/` file. Three output lines and a plan have no second user, and the
rule that has now earned itself three times is to lift when the second user
appears rather than in advance.

No polarity command either. The chapter settles polarity by hand with three
controls before any program is trusted, that was done on Friday 2 October
2026, and a program that re-litigated it would be a program asserting the
thing it was built on.

## The first build, and the one thing it caught

Attempted in WSL on win11 skyhorizon on **Saturday 17 October 2026**. It
failed once, and not where the risk had been placed.

The hand-over said that if anything failed it would fail in `lines.c` and on
a uAPI name, because the structs and ioctls there were written from reading
`include/uapi/linux/gpio.h` rather than from a compile. It did fail in
`lines.c`, one line above the first ioctl, and the uAPI was not the problem.
Every struct field, both attribute ids and all four ioctl names were checked
against that header afterwards and every one of them matches.

What it caught was `O_CLOEXEC`. It is POSIX.1-2008 and `-std=c11` hides it,
so `open(path, O_RDWR | O_CLOEXEC)` was an undeclared identifier under this
volume's gate. `main.c` declares `_POSIX_C_SOURCE 200809L` and `lines.c` did
not.

**That trap was already written down twice in this volume and once in this
journal.** P19's record has `M_PI` hidden by the same mechanism.
`lib/atchat.c` carries two feature macros and a paragraph naming the three
termios symbols that need the second one. And `lib/framebuffer.c`,
`p01/src/main.c` and `p19/src/main.c` all carry the very line that was
missing. I wrote a new file that opens a device and never asked it the
question this volume had already answered four times.

The fix was read off the siblings rather than guessed at, which settled a
second question at the same time. `_POSIX_C_SOURCE 200809L` alone is enough
here: glibc declares `ioctl` in `sys/ioctl.h` with no feature guard, and the
evidence is in this repository, because `lib/i2cbus.h` calls `ioctl` and
compiles clean in P01 and P19 under exactly that macro and nothing else. So
`lines.c` does not need `_DEFAULT_SOURCE`, and adding it defensively would
have been a second answer to a solved question.

## The second build, and a check that was wrong about itself

Rebuilt in WSL on win11 skyhorizon on **Saturday 17 October 2026**, after the
macro above. Five translation units, no diagnostics, and the stricter pass
with `-Wconversion`, `-Wsign-conversion` and `-Wdouble-promotion` added was
clean as well.

The suite ran for the first time: **85 checks, 1 failed.** The failure was in
the suite rather than in the program.

The check asserted that a lit set with rubbish in its high bits drives three
lines. The constant it used was `0xF8`, whose low three bits are all zero, so
the right answer was no line at all and `p15_bits_for` returned exactly that.
The sentence and the constant disagreed with each other, and I had written
the expected value to agree with the sentence instead of computing it from
the constant.

It is now two checks with a value that carries both halves: `0xF8` drives
nothing, and `0xFD` drives green and red, which shows the high bits being
discarded rather than folded in. That is the check the original sentence was
reaching for, and both passed on the rerun.

Worth keeping for what it says about the other eighty four. A suite written
alongside the thing it tests will agree with that thing's defects; what it
cannot do is agree with a value the author never worked out. This one failed
at the first opportunity, loudly, on the one assertion whose arithmetic had
been skipped, which is the behaviour that makes the rest of the output worth
reading.

## What stays a claim, as of Saturday 17 October 2026

**It builds, and both passes are clean.** What it has not done is touch
hardware: no chip has been opened by it, no line requested, no LED lit. The
whole of its device half has been exercised only by being refused, which is
the most a machine with no header can offer.

The corrected pair was rerun on **Saturday 17 October 2026**: 86 checks, none
failed, and the stricter pass clean again afterwards.

The stricter pass was clean on its first attempt, which has not happened in
this volume before without a round of fixes first. The reason is not care, it
is that this program has no floating point anywhere in it: the measured
currents are integer microamps, the only division is by a thousand, and
`-Wdouble-promotion` therefore has nothing to look at. P04 is the only
program here with any `float` at all, and it is the only one that ever had
anything to say to that flag. Worth writing down, because I have twice
predicted that flag would fire on a program that had no floating point in it.

`STATUS.md` leaves P15 at **run**, which is where the two sittings put it.
The program building changes the evidence column and not the state, because
the state of a lab here is what its acceptance test has earned, and the
fourth item still wants three modules seen lit at once by somebody in the
room.

The two acceptance items this unblocks are still unmet. What stood between
them and this lab was a program; what stands between them now is one build and
one sitting with the board, three modules and a lab book. The rails item needs
a voltmeter, and no amount of either will produce one.
