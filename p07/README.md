# P07 and P08: one program for two labs

The host side of labs P07 and P08, in C. One directory, because P08's third
acceptance item is an item about P07.

**What it does.** Reads a `.jsonl` capture written by the firmware and
decides whether it is what its chapter says it is: one JSON object per line,
the same keys from the first line to the last, the keys that shield is
supposed to report, and none of the key that means two streams were merged.

## Why one program

`p01/` serves P01 and P09 because their chapters say the difference between
the two publishers is a bus number. These two are different shields with
different keys, so that reason does not apply. The reason here is that P08's
acceptance test contains this item:

> The numbers live in a different log file from P07.

A check that spans two labs has to be written once, in a place that knows
both. `p07 pair` is that check.

## The rule both labs are built around

P08's intent states it in one sentence:

> two streams, two keys, two log files. Collapsing them into one acceleration
> field is how a wearable-grade number ends up wearing an industrial label.

And its data-flow section names the exact shape of the error: an analysis
cannot find the two streams "already merged in a field called `acc`" and
assume the merge was correct. So **`acc` is refused by name, in all three
profiles**, and the refusal quotes the chapter rather than paraphrasing it.

## Nothing here is an invented format

Every key comes from a chapter. P07 prints its own record:

    {"t":24.1,"rh":41.2,"p":1013.2,"ax":0.02,"ay":0.01,"az":1.00}

and P08's data flow names its two files and their one key each,
`p08_lowg.jsonl` carrying `lowg` and `p08_highg.jsonl` carrying `highg_peak`.
The firmware owns the format; this program only says which keys it looked
for, and `p07 profiles` prints that list.

**One rule is stated in a chapter without a key name**, and it is handled as
a rule rather than as a name. The ILPS22QS has two scales and the chapter
says the scale is part of the record, because "a pressure without the range
it was measured in is a number with two possible meanings". No chapter says
what the scale key is called. So a record carrying a pressure must carry
some key whose name contains `scale`, and inventing the name would have been
inventing a format.

## Two failures that look alike and are not

A capture can be short for two different reasons, and the chapters treat them
as different pitfalls.

- **A schema that drifts.** A key appears half way through a capture, which
  leaves a file no parser reads end to end. The fix is to restart the
  capture, not to patch the file.
- **A capture that was short from its first line.** That is a sensor-hub or
  ISPU mode: those route parts through the LSM6, so the firmware scan finds
  fewer addresses than the shield carries and the record silently loses keys.
  The fix is in the firmware's mode, not in the capture.

The suite carries one scene for each, and the second one has a perfectly
stable schema. A stability check on its own would pass it, which is why the
required-keys check is a separate check rather than a corollary.

## What the parser does and does not reach

It extracts the top-level keys of one JSON object without a JSON library,
because the values are not needed. Three traps are handled and each is
tested: a brace inside a string is not a brace, a key inside a nested object
is not a key of the record, and a line that is not a JSON object is refused
whole rather than half read.

What it does not reach is stated in the source: a string escape is handled
only far enough to keep the parser in step, so a key written `"a\nb"` reads
as `anb`. No key in either chapter has an escape in it.

Key sets are compared **as sets**, not as ordered lists. The rule is "the
same keys from the first line to the last", and a reader parses by name, so
the same names in a different order are the same schema. Order would be a
stricter test of the firmware and a weaker test of the rule.

## State, stated plainly

**Written Sunday 19 October 2026, and not compiled yet.** No shield has been
stacked, no Nucleo flashed, and every capture this program has read is one
its own `scene` wrote.

## Build and check

    make            build p07 and the suite
    make check      run the suite: no shield, no Nucleo, no capture
    make demo       three captures that pass, four that do not, and the pair
    make strict     a second pass with the conversion warnings turned on

## With no shield

    ./p07 profiles
    ./p07 scene --kind lowg --records 50 --out p08_lowg.jsonl
    ./p07 verify --source p08_lowg.jsonl --profile lowg
    ./p07 pair --a p08_lowg.jsonl --b p08_highg.jsonl

## On the bench

    cat /dev/ttyACM0 | tee p07.jsonl
    ./p07 verify --source p07.jsonl --profile iks4a1

## What this program does not decide

Whether the numbers are right. Breathing on the SHT40 to move relative
humidity, and a palm over the STTS22H to move it about half a degree, are
items a person checks with a shield in front of them. Nor does it see the
I2C bus: every one of those parts sits on the STM32 bus and not on the Pi's,
which is the boundary P07 exists to teach, and the empty `i2cdetect` grid on
the Pi is a pass.
