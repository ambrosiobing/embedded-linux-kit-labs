# P19: the offline tilt meter

Six bytes in, two angles out, two bars on a panel. There is no branch in
this program that reaches a network, and that is the chapter's point rather
than an omission.

## What is new here, and what is not

Almost nothing is new, and that is the result worth reporting. The sensor is
`lib/adxl345.h`, which P01 and P09 also read. The panel is
`lib/framebuffer.h`, which P06 also paints. The bus is `lib/i2cbus.h`. What
belongs to this lab is two formulas and a drawing, which is exactly what its
chapter says the product is.

Writing it was therefore a test of whether the library had the right shape,
and the answer was no twice: the accelerometer decode and the framebuffer
both had to be lifted out of the labs that owned them first, and the i2c
sequence after that. Three lifts for one small program is the cost of having
guessed at nothing in advance.

## State, stated plainly

**Compiled and run against invented gravities, never run against a sensor or
a panel.** No assembly was rotated. The bars have never been on glass.

## Build and check

    make            build p19 and the suite
    make check      run the suite: no hardware, no sensor, no panel
    make demo       the acceptance test with nothing to rotate
    make strict     a second pass with the conversion warnings turned on

## With no hardware

    ./p19 angles --bytes "00 01 00 00 00 00"     # on its side: 90 of pitch
    ./p19 sweep --axis pitch
    ./p19 sweep --axis roll

## With the sensor and the panel

    sudo i2cdetect -y 1
    ./p19 run --bus 1 --fb /dev/fb0 --fb-sysfs /sys/class/graphics/fb0

## The acceptance test is two statements, not one

Rotate the assembly ninety degrees on one axis and that bar travels full
scale. **And the other bar stays near zero.** The second half is the one
worth building a program around, because it is the half a per-axis
instrument fails.

`sweep` is that test with nothing to rotate. It tilts a gravity about one
axis in steps, reports how far the swept angle went and how far the other
one moved, and fails if the other one moved at all. On this arithmetic it
moves by nothing: not nearly nothing, exactly nothing, because pitch takes
`ax` against the combined length of `ay` and `az`, and rotating about the x
axis moves acceleration between those two without changing their combined
length.

## The fold at ninety degrees

Both formulas take the arctangent against a positive square root, so both
fold: **a hundred and twenty degrees of pitch reads the same as sixty.**

That is a limit of the instrument rather than a defect, and it is in the
suite so that nobody later reads it as one. The acceptance test asks for
ninety degrees, so the range is exactly enough and no more, and a reader who
wants a full turn needs a different instrument rather than a wider scale.

## What the drawing refuses

**To run off the track.** An angle past full scale rests on the end rather
than running off it, which is a reading rather than a drawing accident.

**To write outside the panel.** Every rectangle goes through the shared
framebuffer, which clips, and the suite paints both bars at their ends and
then at four thousand degrees into a file with a guard area past the end of
the geometry, which must come back untouched.

**To squeeze itself onto a panel too small to read.** Under forty pixels
wide or twenty four high and the layout is refused rather than drawn one
pixel high.

## What each file is

| File | What it holds |
| --- | --- |
| `src/p19.h` | every declaration, and the fold as a comment |
| `src/angles.c` | the two formulas, and the sweep that is the acceptance test |
| `src/bars.c` | two tracks, two centre marks, two clamped markers |
| `src/main.c` | four subcommands, none of which opens a socket |
| `tests/test_p19.c` | the suite, four groups, no framework |
| `../lib/adxl345.h` | the sensor, shared with P01 and P09 |
| `../lib/framebuffer.h` | the panel, shared with P06 |
| `../lib/i2cbus.h` | the bus, shared with P01 |

## Exit status

Zero means the question was answered. Two means the command line was wrong.
One means the question could not be answered, with the reason on standard
error, or that a sweep failed its own acceptance item.
