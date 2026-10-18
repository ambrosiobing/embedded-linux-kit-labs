# P10: the energy characterisation bench

The host side of lab P10, in C. Three states, two numbers each, and one
comparison that decides whether any of it means anything.

**What it does.** Reduces a PPK2 capture to a median and a p95 without
holding it in memory, keeps them in a lab book, and judges a run against the
chapter's acceptance test: three states that are actually distinct, a repeat
that reproduced, and the sentence that keeps a dev-board figure from being
quoted as a bare-module one.

## The comparison this lab exists for

The chapter's fourth acceptance item says that if the three traces overlay,
the DUT is still USB-powered, and that the cause is the cable rather than the
instrument. That is the failure worth a program. An ESP32 that is also
plugged into USB draws most of its current through the USB rail, and the PPK2
then reports the small remainder that happens to come through its own output.
Traces taken that way look reasonable and mean nothing.

`p10 judge` refuses that run and says what to check first.

## No threshold of mine, anywhere

Two states are distinct when the quieter one's p95 sits below the busier
one's median: ninety five per cent of what the quiet state draws is below
what the busy state typically draws. The repeat of step 7 holds when neither
capture's median has moved past the other's p95.

Both statements are made entirely out of measured quantities. There is no
separation factor, no tolerance and no percentage anywhere in this program
for a reader to argue with, or for a bench to be quietly tuned against. That
is the volume's first rule and it is the reason the comparison is shaped this
way rather than as a ratio.

## The order is measured, not read off the names

Nothing here believes the state called "sleep" is the quietest. `judge` sorts
by measured median and prints the order it found, because a bench where deep
sleep is not the quietest is exactly the bench this lab exists to catch. The
suite asserts it with the names deliberately wrong.

The same applies to the sentence step 9 asks for: it is required of the state
that **measured** quietest, because that is the figure somebody will later
quote as a bare-module deep-sleep current.

## What the quantiles cost

Two seconds at 100 kS/s is two hundred thousand samples, which do not fit a
sorted array in this volume's budget. So the quantiles come from a fixed
histogram of 4096 buckets in two passes, one to learn the range and one to
fill the counts, and nothing allocates.

What that costs is resolution. Every quantile is a bucket midpoint, so every
reduction carries its bucket width and every figure is printed with it. A
quantile reported without its resolution is a number with a decimal place it
has not earned.

It also means a capture arrives as a path and never on standard input, which
other labs here do accept: a stream can be read once, and a second pass needs
a file. That is refused by name rather than worked around.

## State, stated plainly

**Written Sunday 18 October 2026, and not compiled yet.** No PPK2 has been
connected, no ESP32 flashed, and no capture taken. Every trace this program
has seen is one its own `scene` wrote.

The three state kinds in `scene` are shaped the way an ESP32 is shaped rather
than the way a tidy test would be. In particular BLE advertising does not sit
at the same floor as radio-off idle, because advertising keeps the radio
subsystem powered. A generator where the two shared a baseline would fail the
separation test for a reason about firmware rather than about a cable, which
is not what this lab's refusal means.

## Build and check

    make            build p10 and the suite
    make check      run the suite: no instrument, no DUT, no trace
    make demo       a run that passes, and the run this lab exists to refuse
    make strict     a second pass with the conversion warnings turned on

## With no instrument

    ./p10 scene --kind sleep --samples 20000 --out sleep.csv
    ./p10 reduce --source sleep.csv --state sleep --book run.p10book \
        --why "a dev-board, so the USB-UART silicon leaks"
    ./p10 report --book run.p10book
    ./p10 judge  --book run.p10book

`--kind usb` is the one to look at. It is what a DUT with its own USB cable
still connected looks like through the instrument, and writing it three times
under three state names produces the run `judge` exists to refuse.

## On the bench

Capture with Nordic's nRF Util or the published ppk2-python helpers, whichever
landed on the Pi, then point `reduce` at the file and name the column:

    ./p10 reduce --source sleep.csv --state sleep --column 1 \
        --book run.p10book --why "..."

The vendor's tool owns the export format. This program only says which column
it was told to read, counts every line that was not a sample, and prints that
count beside every result.

## What this program does not decide

Whether these are the right figures for this board. The first two acceptance
items ask whether deep-sleep is tens of microamps to low milliamps and whether
the active radio is tens of milliamps, and those are judgements against a part
and its datasheet rather than against a capture. `judge` prints the figures and
the order and says plainly that it is not ruling on them.

It also does not clamp a negative reading to zero. The PPK2 can report small
negatives in the noise around a true zero, and clamping them would move a
median upward by an amount nobody could see afterwards. They are refused and
counted, and the count is on every line of output.
