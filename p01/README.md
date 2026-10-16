# P01: the shock event publisher, which P09 also runs

Two chapters, one program. P01 runs it on a Pi 4 with the accelerometer on
bus 1 and a Cat-4 modem carrying the publishes. P09 runs it on a NanoPi NEO
Air with the same sensor on bus 0, headless, appending to soldered flash. The
chapters say the difference is the bus number, so the difference here is an
argument rather than a second program.

**What it does.** Decodes six bytes from an ADXL345 into three accelerations
and a magnitude, decides whether that magnitude is far enough from one
gravity to be an event, publishes at most one event every two seconds however
long the table rings, and works out in advance what an hour of that costs a
radio and a flash.

## State, stated plainly

**Compiled and run on captures this program generated, never run against a
sensor.** No acceleration in any test here was measured. The live read is
compiled on every Linux machine the checks run on, and has never had a bus to
answer it.

## Build and check

    make            build p01 and the suite
    make check      run the suite: no hardware, no bus, no sensor
    make demo       five benches that were never knocked, and the budget
    make strict     a second pass with the conversion warnings turned on

## With no sensor

    ./p01 decode --bytes "00 00 00 00 00 ff"      # upside down, not an event
    ./p01 scene --kind ringing --out ringing.samples
    ./p01 replay --source ringing.samples
    ./p01 budget --rate 20 --cooloff 2 --fsync 30 --record 48 --hours 24

## With the sensor

    sudo i2cdetect -y 1          # on the Pi 4, for P01
    sudo i2cdetect -y 0          # on the NanoPi NEO Air, for P09
    ./p01 run --bus 1 --seconds 60

The bus number is the one thing you must not copy from the other chapter. A
command written for the Pi and run on the NanoPi reports an empty bus, and
the sensor is not at fault.

## The decision is on the magnitude, never on an axis

This is the lab's own rule and the suite is built around it.

A sensor lying on its side still reads one gravity: it has simply moved to a
different axis. A threshold on any single axis calls that an event, so a
board picked up and set down differently publishes a shock that never
happened.

Free fall is the opposite error. Zero gravities is exactly as far from rest
as two gravities, in the other direction, and a threshold that watches only
for a high reading misses a board falling off a bench entirely.

Both fall out of one comparison, on the distance of the magnitude from one
gravity, and both are in the suite as scenes: ten seconds on its side
produces nothing, and free fall produces events.

## The sign extension

Two bytes arrive low first and they are two's complement. Minus one gravity
is `0xFF00`, not 65280 counts of acceleration. A missed sign extension here
produces a magnitude that is enormous and an event on every sample, which
looks exactly like a broken sensor.

The suite checks both ends: `00 00 00 00 00 ff` is minus one gravity on z,
whose magnitude is still one, which is what makes lying upside down not an
event.

## The budget, which is P09's question

On the NanoPi the cool-off is not only a bandwidth control. Every event that
is appended to the log is a write to flash soldered to the board, and that
flash cannot be replaced when it tires. So the arithmetic is worth doing
before the run:

    an hour at 20 Hz, ringing continuously:   72000 samples
    without the cool-off:                     72000 events, 3.3 MB
    with a two second cool-off:                1800 events, 0.08 MB

A factor of forty, from one timestamp. And the fsync figure is the other half
of it: committing on a timer rather than on every sample gives **fewer than
one fsync per event**, and the program says so in those words. An fsync
interval shorter than the cool-off gives more than one, which is a write to
flash with nothing new in it, and the suite checks that case too.

## What each file is

| File | What it holds |
| --- | --- |
| `src/p01.h` | every declaration, and the rules above as a comment |
| `src/adxl.c` | six bytes into three accelerations, and the bus |
| `src/policy.c` | the dead band, the cool-off, the budget and the replay |
| `src/scene.c` | benches that were never knocked, including two that must stay quiet |
| `src/main.c` | five subcommands, one of which needs hardware |
| `tests/test_p01.c` | the suite, five groups, no framework |
| `../lib/linereader.h` | the line reader, shared with P03, P05, P06, P13 and P18 |

## What the program refuses

**To interpolate a sample it could not read.** A failed read on the bus is
counted and reported. Filling it in would hide a fact about the bus inside a
fact about the table.

**To decide on one axis.** See above. This is the rule the two quiet scenes
exist to hold it to.

**To run without a bus number.** `--bus` has no default, because the only
two values it takes belong to different chapters and a wrong one looks like
a missing sensor.
