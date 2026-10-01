# P04: the host half of the vibration and ultrasound gateway

The Linux side of lab P04, in C, written and tested before the probe is wired.
Every command runs with no hardware attached, so the pipeline, its tests and
its defects are all dealt with while the probe is still in its box. When it
arrives, the remaining work is one USB cable and a real baseline.

**What it does.** Reads sequence-numbered blocks of two channels, wideband
vibration and a digital microphone, accounts for holes in the stream, windows
and transforms each block, sums a named band, and classifies the result
against a baseline you measured as NORMAL, WARNING or FAULT.

## State, stated plainly

**Written, never compiled here, never run against hardware.** The authoring
laptop has no C compiler and no WSL distribution, so the first compile happens
in the workflow on a push. A red run there is the expected way to find out what
the compiler has to say, not a surprise. No number this program produces is a
measurement of anything physical: the synthetic stimuli are signals with the
right shape, not a model of the probe, and a threshold tuned against them is a
threshold tuned against them.

## Build and check

    make            build p04 and the suite
    make check      run the suite: no hardware, no serial port, no network
    make demo       the lab's own demonstration, end to end, on synthetic data
    make strict     a second pass with the conversion warnings turned on

The gate is `-Wall -Wextra -Werror` with a few more. The conversion warnings
live in `make strict` instead, because code that turns counts into floats and
back deserves a reading rather than a reflex cast, and mixing that into the
gate teaches people to silence it.

## With no hardware

    ./p04 synth --kind idle   --blocks 10 --out idle.p04
    ./p04 synth --kind touch  --blocks 10 --out touch.p04
    ./p04 synth --kind speech --blocks 10 --out speech.p04

    ./p04 baseline --source idle.p04 --out book.lab \
        --mounting "synthetic, no probe" --warn 1.5 --fault 3.0 \
        --band-low 2000 --band-high 6000

    ./p04 validate --source idle.p04   --book book.lab
    ./p04 analyse  --source touch.p04  --book book.lab
    ./p04 analyse  --source speech.p04 --book book.lab
    ./p04 replay   --source touch.p04  --book book.lab

The last three reproduce the lab's own demonstration. A case touch raises the
vibration band with the microphone unmoved; a voice raises the microphone an
order of magnitude with the band unmoved. That separation is the lab, and the
suite asserts it as a comparison a reader can repeat rather than a sentence
they have to believe.

## Why it is C

Because the lab is embedded Linux and the deliverable is the program, not a
description of one. Three choices follow from that and are visible in the
source.

**No allocation on the data path.** Every buffer is a fixed array sized at
compile time. A capture whose blocks are longer than the build allows is
refused at the file header rather than part way through an analysis.

**Library code returns a status and never exits.** Only `main.c` decides the
program stops, which is what lets the same code sit behind a service later.

**Samples are `int16_t` because that is what the probe sends.** They become
`float` only inside the transform, so the one place where a count turns into a
number is small enough to check.

## What each file is

| File | What it holds |
| --- | --- |
| `src/p04.h` | every declaration, and the three rules above as a comment |
| `src/block.c` | the capture file format and the transport accounting |
| `src/dsp.c` | the window, an iterative radix-2 transform, the band |
| `src/analysis.c` | thresholds, classification, the gravity invariant, the run |
| `src/labbook.c` | the baseline file, flat key and value, no dependency |
| `src/synth.c` | synthetic stimuli, deterministic from a seed |
| `src/main.c` | six subcommands and the exit status rules |
| `tests/test_p04.c` | the suite, four groups, no framework |

## Three things the program refuses to do

**Classify over a stream with holes in it, silently.** The sequence numbers are
accounted for first and reported beside every result, because a spectrum over a
stream that lost a third of its blocks is a picture of the holes.

**Use a band that sits above Nyquist.** It returns an error rather than an
empty sum. A sum of no bins reads as silence, and silence is the wrong answer
to a question that should not have been asked.

**Apply a lab book to a capture that arrived at a different rate.** The band
edges were turned into bin numbers at the rate the baseline was taken at.
Pointing them at data from a different rate gives an answer that is confident
and wrong, so `analyse` refuses and says by how much the rates differ.

## Exit status

Zero means the question was answered. Non-zero means it could not be, and the
reason is on stderr. A classification of FAULT is still a zero exit: the
program worked and the machine did not, and conflating those two is how a
monitoring tool teaches its operator to ignore it.

## What is still missing

A `live` subcommand that opens the probe's character device with `termios` and
feeds the same pipeline. It is deliberately absent until the probe is wired,
because a serial reader with nothing on the other end is untested code that
looks finished.

The probe's own framing comes from its firmware and is not invented here. The
capture format in `src/block.c` is this program's own file format for recorded
blocks, and it says so.
