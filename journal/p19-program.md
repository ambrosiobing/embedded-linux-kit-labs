# P19: the last chapter, and what writing it said about the library

The lab is [SPI LCD tilt meter](../chapters/19-spi-lcd-tilt-meter.md), and
its program was written on **Saturday 17 October 2026**. It is the last
chapter in this volume that printed Python, and the volume now prints none.
The lab's other record, [p19.md](p19.md), is about the header this bench
does not have, which is still true and is why no amount of software moves
this one past **checked**.

## What was new, which was almost nothing

Two formulas and a drawing. That is the whole of what belongs to this lab,
and it is exactly what its chapter says the product is.

The sensor is `lib/adxl345.h`, which P01 and P09 read. The panel is
`lib/framebuffer.h`, which P06 paints. The bus is `lib/i2cbus.h`. A reader
could reasonably ask what there was to write.

## Which made it a test of the library, and the library failed it three times

None of those three files existed when this lab was started. Each had to be
lifted out of the program that owned it, and each lift is a place where the
shape had been guessed wrong:

**The accelerometer decode** lived in P01, which owns the shock publisher.
It is a property of the part.

**The framebuffer** lived in P06, which owns the ranging kiosk. The geometry,
the mapping and the clipping belong to anything that draws; what stayed in
P06 is the part that knows it is painting an 8x8 grid of zones.

**The i2c sequence** lived in P01 as well. This one was caught by writing
the first line of P19's `run` and finding that it needed to include a header
from another lab's source directory, which would have been the first
cross-lab include in the volume. One function is not worth setting that
precedent for.

Three lifts for one small program. That is the bill for having guessed at
nothing in advance, and it is cheaper than it looks: every one of them was
mechanical, every one was verified against the suite of the lab it came
from, and two of them turned up defects on the way.

## The two formulas, and the half of the test that matters

The acceptance test is two statements. Rotate ninety degrees on one axis and
that bar travels full scale. **And the other bar stays near zero.**

The second is the one worth a program. Pitch takes `ax` against the combined
length of `ay` and `az`, so rotating about the x axis moves acceleration
between those two without changing their combined length at all. Roll
therefore does not move: not nearly, exactly. `sweep` is that test with
nothing to rotate, in a hundred and eighty one steps and again in eighteen
hundred and one, and the worst the other angle reaches is zero.

A per-axis instrument fails that half, and it fails it in the direction
nobody notices: the bar twitches, and a reader decides the sensor is noisy.

## The fold, which is a limit rather than a defect

Both formulas take the arctangent against a positive square root, so both
fold at ninety degrees. A hundred and twenty degrees of pitch reads the same
as sixty.

That is in the suite deliberately, so that the next reader finds it as a
property rather than discovering it as a bug. The acceptance test asks for
ninety, so the range is exactly enough and no more, and a reader who wants a
full turn needs a different instrument rather than a wider scale.

## Two things caught before the compiler saw them

`M_PI` is an X/Open extension, and `-std=c11` hides it. P04 already carries
the guard for that, and the second reader of a problem should use the first
reader's answer rather than inventing one.

And a comment that asserted a number I had not checked. I wrote that
`180.0 / 0.1` comes to 1799.9999999999998 in binary, as the justification
for rounding a step count. It does not; it comes to 1800.0000000000002, so
truncating would have worked and my reason was false. The rounding stayed
because it is right for the general case, and the comment was rewritten to
say what is actually true. A plausible number in a comment is still a claim.

## The first compile, and the end of a road

Built on Saturday 17 October 2026: no diagnostics under the gate, 41 checks
with none failed, and the stricter conversion pass clean. P01 was rebuilt on
the same commit and is unchanged at 55, which is what the bus moving out of
it was supposed to look like.

## What stays a claim

`STATUS.md` moves this row to **checked**, and that is the end of the road
rather than a step along it.

Unlike every other lab here, there is nowhere further to go. This lab needs
a 2x20 stacking header or a tall header
whose pins clear the panel's socket, and this bin has neither, so P19 can
reach **checked** and stop there. The program being finished changes nothing
about that, which is worth stating plainly in the same place as the program:
software was never what this lab was short of.
