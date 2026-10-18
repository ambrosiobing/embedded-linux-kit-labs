# P10: the first lab where the no-threshold rule decided the design

The lab is [PPK2 energy characterisation bench](../chapters/10-ppk2-energy-characterisation-bench.md)
and its program was written on **Sunday 18 October 2026**.

Four acceptance items, and the first two are not this program's business:
whether deep-sleep is tens of microamps to low milliamps and whether an
active radio is tens of milliamps are judgements about a part against its own
datasheet. What is left is a median and a p95 per state, which is arithmetic,
and one comparison, which is the lab.

## The comparison is the lab

> If the three traces overlay, the DUT is still USB-powered. That is a fail,
> and the cause is the cable, not the instrument.

An ESP32 that is also plugged into USB draws most of its current through the
USB rail, and the instrument then reports the small remainder that happens to
come through its own output. The chapter says it plainly: traces taken that
way look reasonable and mean nothing.

That is a failure a table cannot show and a person reading three tidy rows
will not notice, which is the whole argument for a program here.

## Where the first rule stopped being a formality

**No default threshold anywhere** is this volume's first rule, and in the
labs so far it has mostly meant not inventing a number that a lab book should
have supplied. Here it bit.

The obvious way to decide whether three traces overlay is a ratio: refuse the
run when the loudest median is less than some factor above the quietest. Every
version of that sentence has a number of mine in it, and a bench can be tuned
against a number until it passes.

What the rule forced is a comparison made entirely out of measured
quantities: **two states are distinct when the quieter one's p95 sits below
the busier one's median.** Ninety five per cent of what the quiet state draws
is below what the busy state typically draws. There is nothing in that
sentence to argue with and nothing to tune against, and it says something
stronger than a ratio would: not that the numbers differ, but that the
distributions do not meet.

The repeat of step 7 took the same shape. It holds when neither capture's
median has moved past the other's p95.

Both are asserted from both sides in the suite, at the exact boundary: a p95
equal to the next median is not separated, and one nanoamp below it is. A
comparison tested from one side is a direction rather than a rule.

## The order is measured, and the suite lies to it on purpose

Nothing in the judge believes the state called `sleep` is the quietest. It
sorts by measured median and prints the order it found, because a bench where
deep sleep is not the quietest is exactly the bench this lab exists to catch.

The suite asserts that with the names deliberately wrong: it builds a book
where `sleep` draws ninety milliamps and `ble` draws forty five microamps,
and checks the judge puts them in the measured order. The sentence step 9
asks for is likewise required of the state that **measured** quietest, since
that is the figure somebody will later quote as a bare-module deep-sleep
current.

## What no allocation cost, and where that cost is printed

Two seconds at 100 kS/s is two hundred thousand samples. Nothing in this
volume allocates on the data path, so a sorted array was never available, and
the quantiles come from a fixed histogram of 4096 buckets filled in two
passes.

The cost is resolution: every quantile is a bucket midpoint. Rather than hide
that, every reduction carries its bucket width and every figure is printed
with it. A quantile reported without its resolution is a number with a
decimal place it has not earned.

It also costs standard input. Other labs here read a stream; a second pass
needs a file, so a capture arrives as a path and `-` is refused by name
instead of quietly reading half of it twice.

The suite checks the quantiles against arithmetic rather than against the
program. The `ramp` scene writes 1, 2, 3 microamps and so on, so for a
thousand samples the span is 999001 nanoamps, the bucket is ceil(999001 /
4096) which is 244, the median is the 500th sample and the p95 the 950th. Each
is asserted to be within one bucket of the value computed that way, and the
bucket itself is asserted exactly. The same arithmetic is then checked at a
second size, a hundred samples, so the derivation is not being tested on the
one case it was derived from.

## A negative reading is refused, not clamped

The PPK2 can report small negatives in the noise around a true zero. Clamping
those to zero would move a median upward by an amount nobody could see
afterwards, which is repairing data, and this volume does not repair.

So they are refused and counted, and the refused count prints beside every
result. The limitation that leaves is stated rather than hidden: a capture
where this matters is a capture whose refused count is large, and that number
is on every line of output. None of the three states this lab measures sits
at a true zero.

## The generator is shaped like an ESP32, which took a correction

The first version of the three state scenes gave BLE advertising the same
baseline as radio-off idle, with bursts on top. It is a tidy thing to write
and it fails the separation test, because the two medians land on top of each
other.

That failure would have been real and about the wrong thing. Advertising keeps
the radio subsystem powered, so its floor sits above radio-off idle, and a
generator that says otherwise produces a refusal about firmware where this
lab's refusal is about a cable. The scenes now put BLE's baseline above idle's
and its bursts above that again.

## One lift

`lib/thousandths.h`, which is P15's fixed-point formatter with the units
taken out of it. P15 prints a measured current table in milliamps from
microamps; this lab prints quantiles in microamps from nanoamps and in
milliamps from nanoamps. Same arithmetic, carry included, so it moved rather
than being copied, and P15's suite keeps testing it, including the carry case
P15 found on its own first run.

## What stays a claim, as of Sunday 18 October 2026

**None of this has been compiled.** No PPK2 has been connected, no ESP32
flashed and no capture taken. Every trace this program has seen is one its
own `scene` wrote. `STATUS.md` leaves P10 at **written** and says in the same
row that the program is in the tree and has never been built.
