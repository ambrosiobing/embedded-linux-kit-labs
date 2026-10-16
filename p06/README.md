# P06: the host half of the 8x8 time-of-flight occupancy kiosk

The Linux side of lab P06, in C, written and tested before the sensor is
stacked. Sixty-four distances arrive on one line and sixty-four rectangles are
painted, and between those two there is one comparison per zone against a
threshold measured with the field of view empty.

**What it does.** Reads `Z,d00,...,d63` lines from a file, from standard input
or from a character device. Parses them strictly or drops them with a reason.
Measures the wall, refusing to call the result a threshold if the measurement
does not deserve the name. Classifies each zone against it. Paints an 8x8 grid
into a framebuffer through `mmap`, clipped to a geometry it read rather than
assumed.

## State, stated plainly

**Compiled and run on synthetic frames, never run against a sensor.** No
distance here came from a VL53L8CX, no rectangle here has been on glass, and
the Nucleo has never been flashed with the firmware this program expects. What
the suite proves is that the program agrees with its own arithmetic, which is
worth having and is not a measurement.

## Build and check

    make            build p06 and the suite
    make check      run the suite: no hardware, no display, no sensor
    make demo       the chapter's own sequence, with files standing in
    make strict     a second pass with the conversion warnings turned on

## With no sensor and no display

    ./p06 synth --frames 30 --scene empty --out empty.stream
    ./p06 baseline --source empty.stream --out bench.book \
        --placement "synthetic, no sensor"
    ./p06 show --source empty.stream --book bench.book --frames 1
    ./p06 synth --frames 30 --scene hand --hand-zones 6 --out hand.stream
    ./p06 run --source hand.stream --book bench.book --expect-zones 4:10

`show` prints the 8x8 grid as text, which is the chapter's instruction to
confirm the numbers before painting anything, and it needs no display of any
kind.

## With the sensor and the LCD

    stty -F /dev/ttyACM0 115200 raw
    ./p06 baseline --source /dev/ttyACM0 --out bench.book \
        --placement "kiosk bracket, 1.8 m from the far wall"
    ./p06 run --source /dev/ttyACM0 --book bench.book \
        --fb /dev/fb0 --fb-sysfs /sys/class/graphics/fb0

The geometry comes from the two text files the kernel publishes next to the
device, so you can run `cat` on the same two files and see the numbers the
program is using. An `ioctl` would not allow that, and it would need a
Linux-only header for a program that otherwise does not.

## Four things the program refuses to do

**Paint a frame it did not parse.** The paint happens inside the parse loop
and nowhere else. A sensor that stops sending stops the repaint, rather than
leaving a frame on the glass that is a minute old and looks live. The suite
counts frames parsed against times the glass was written and requires them to
match, and runs a stream with no frame in it over a glass that must come back
byte for byte unchanged.

**Repair a line.** Sixty-five fields, the first the literal `Z`, the rest
decimal millimetres within the sensor's range. Anything else is dropped and
counted under one of four reasons, because a short line and a line from the
ST-LINK node are different problems. A partial frame painted on the glass
looks like a real reading and is not.

**Produce a threshold from a measurement that is not one.** `baseline`
refuses fewer than ten frames, and refuses a baseline whose own frames
disagree by more than 20 mm, which is the stability the chapter's acceptance
test asks for. A wall that moved during the measurement means something was in
the field of view or the sensor was not still, and the number it would produce
is not a threshold.

**Write outside the mapping.** Every rectangle is clipped to the geometry, and
a rectangle entirely outside paints nothing rather than erroring. The suite
paints into a file with a guard area past the end of the geometry and requires
every byte of it to come back untouched.

## Two conventions, written down because they are not measurements

**Zone 0 is painted at the top left and the zones run row by row.** That is a
choice this program fixes, not something read off the sensor. Confirm it at
the bench by putting a hand in a known corner, and if it is wrong the fix is
in the firmware's print order or in one loop here, not in both.

**A zone reporting zero millimetres saw no target.** It is not a target at
zero distance. Treating it as one would make an empty room read as something
touching the glass, which is wrong in the most alarming direction, so a zero
is never the nearest thing in the room and never an occupied zone.

## What each file is

| File | What it holds |
| --- | --- |
| `src/p06.h` | every declaration, and the rules above as a comment |
| `src/frame.c` | the contract, parsed strictly, and the four ways to fail it |
| `src/book.c` | the measured wall, the threshold, and the two refusals |
| `src/fb.c` | the geometry, the mapping, the clipping and the colours |
| `src/synth.c` | frames with known contents and lines wrong in known ways |
| `src/run.c` | the loop, which is the only thing that paints |
| `src/main.c` | four subcommands and the exit status rules |
| `tests/test_p06.c` | the suite, six groups, no framework |
| `../lib/linereader.h` | the line reader, shared with P18, header only |

## Exit status

Zero means the question was answered. Two means the command line was wrong.
One means either the question could not be answered, with the reason on
standard error, or a gate was applied and failed. Two gates exist and both are
the chapter's acceptance test rather than an invention: `baseline` refuses an
unstable wall, and `run` fails when `--expect-zones` is given and the busiest
frame falls outside it. Without that option `run` reports and exits zero,
because counting is not judging.

## What the firmware still owes

The sketch on the Nucleo, printing one line per frame in millimetres. This
half assumes nothing else about it, and deliberately does not invent the
sensor's own frame format: the ST driver owns that, and the chapter says not
to guess at it.
