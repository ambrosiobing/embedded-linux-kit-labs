# P18: the host half of the USB-CDC recorder

The Linux side of lab P18, in C, written and tested before the Nucleo is
plugged in. The lab is one number with a second number beside it: how many
samples arrived, and how many did not. Everything here exists to make those
two honest.

**What it does.** Reads a line-oriented stream from a file, from standard
input or from a character device, takes the leading decimal field of each
line as a sequence number, accounts for gaps, duplicates and reorders, writes
the lines it counted, and measures how long the capture took. Then it applies
the chapter's acceptance test and says which part of it failed.

## State, stated plainly

**Compiled and run on synthetic streams, never run against a Nucleo.** The
authoring laptop has no compiler it may use, so the first compile happens in
the workflow on a push. No number here was measured on hardware. The streams
the suite reads were written by the program a moment earlier.

Unlike the sibling lab, this one has nothing missing for the hardware. A
character device is one more source, and `stty` sets the line discipline
before the program opens it, so the day the Nucleo arrives the same command
runs against `/dev/ttyACM0` with the source changed and nothing else.

## Build and check

    make            build p18 and the suite
    make check      run the suite: no hardware, no serial port, no network
    make demo       the lab's own demonstration, end to end, on synthetic streams
    make strict     a second pass with the conversion warnings turned on

## With no hardware

    ./p18 synth --samples 2000 --out demo.stream
    ./p18 sink --source demo.stream --out demo.capture --report demo.p18 \
        --seconds 0 --rate 1000
    ./p18 count --source demo.capture
    ./p18 verify --report demo.p18 --allow-unmeasured-rate

    ./p18 synth --samples 2000 --drop 10,11,12,13 --out holed.stream
    ./p18 sink --source holed.stream --report holed.p18 --seconds 0
    ./p18 verify --report holed.p18 --allow-unmeasured-rate   # exits 1

## With the Nucleo

    stty -F /dev/ttyACM0 921600 raw
    timeout 15 ./p18 sink --source /dev/ttyACM0 --out p18_capture.txt \
        --report p18_run.p18 --seconds 10 --rate 1000
    ./p18 verify --report p18_run.p18

`timeout` is not decoration. The deadline is tested after every line, so a
stream that slows down still stops on time, but a stream that stops entirely
leaves the program waiting inside a read with no line to come back from.
Fifteen seconds around a ten second capture is the margin.

## The two faults this lab refuses to combine

A stream can fail in two independent ways and they have different causes.

**Loss.** Sequence numbers that never arrived. Caused by the host being busy,
by a shared USB controller, by a hub, or by a reader that cannot keep up.

**Rate.** A stream that runs below the rate it was meant to. Caused by the
firmware spending too long between samples, most often in a formatted print.

A capture at half the intended rate with no gaps at all is a healthy transport
in front of a slow source, and a capture at the right rate with holes in it is
the opposite. One number cannot say which happened, so there are two, they are
printed on separate lines, and `verify` reports them separately even when both
pass. A third line reports integrity, which is whether the recorder had to
throw anything away in order to produce the capture at all.

## Why it is C

Because the lab is embedded Linux and the deliverable is the program. Three
choices follow, and they are the same three the sibling lab is written
against.

**No allocation on the data path.** Every buffer is a fixed array sized at
compile time.

**Library code returns a status and never exits.** Only `main.c` decides the
program stops.

**Nothing is repaired.** A gap is counted and left as a gap. A sink that
interpolates over a hole has discarded the only measurement the lab makes.

## What each file is

| File | What it holds |
| --- | --- |
| `src/p18.h` | every declaration, and the rules above as a comment |
| `src/lines.c` | the line reader, and the one field this program parses |
| `src/synth.c` | synthetic streams with faults placed by sequence number |
| `src/run.c` | the capture loop, the monotonic clock and the rate question |
| `src/report.c` | the run report file, and the three-part gate |
| `src/main.c` | four subcommands and the exit status rules |
| `tests/test_p18.c` | the suite, six groups, no framework |
| `../lib/seqtrack.h` | gaps, duplicates and reorders, shared with P04, header only |

## Three things the program refuses to do

**Split a line that did not fit.** A line longer than the buffer is counted
and discarded whole. Handing back the first part and then the remainder would
turn one over-long line into two samples, the second of which nobody sent.
Losing a sample is a measurement. Inventing one is a lie in the same column.

**Claim a rate it did not measure.** A capture file on a disk is read in
milliseconds. Samples divided by that is a statement about this program, not
about the probe, so the report carries a flag saying whether the source was
running in real time, and `verify` refuses an unmeasured rate unless it is
told in as many words to let it through.

**Interpolate.** Not over a gap, not over a malformed line, not over a line
that arrived late.

## Exit status

Zero means the question was answered. Two means the command line was wrong.
One means either the question could not be answered, with the reason on
standard error, or the acceptance test was applied and failed.

That last part is a deliberate difference from P04, where a classification of
FAULT is still a zero exit because the program worked and the machine did
not. `verify` is not a classifier. It is the acceptance test, it exists to be
put in front of something, and a gate that returns success when it did not
pass is not a gate. `sink` and `count` report and exit zero whatever the
numbers say.

## What the firmware still owes

The sketch on the Nucleo. This half assumes a line that begins with a decimal
sequence number and is followed by whatever the firmware prints, and it
interprets nothing after the first field. That is the whole contract, and the
chapter's step 1 is still a step.
