# P03: the host half of the Cat-M node that stamps its payload from GNSS

The Linux side of lab P03, in C, written and tested before the HAT is
seated. The product of this lab is an ordering rather than a number, so most
of this program is about making that ordering true instead of describing it.

**What it does.** Locks the bearer and confirms it rather than assuming it,
powers the GNSS receiver, reads a fix, powers the receiver down, and only then
opens the MQTT session and publishes. Keeps the last fix so a cycle without a
new one republishes it with the stamp it came with. Skips a cycle that has
neither.

## State, stated plainly

**Compiled and run against transcripts, never run against a module.** No SIM,
no antenna, no sky. Every reply came from a file this program wrote a moment
earlier.

## Build and check

    make            build p03 and the suite
    make check      run the suite: no hardware, no serial port, no SIM
    make demo       the scenes replayed, and the ordering audited
    make strict     a second pass with the conversion warnings turned on

## With no module

    ./p03 transcript --scene fix --out fix.transcript
    ./p03 session --transcript fix.transcript
    ./p03 transcript --scene wrong-bearer --out bearer.transcript
    ./p03 session --transcript bearer.transcript      # exits 1

## With the HAT

    ./p03 session --port /dev/ttyUSB2 --cycles 10 --log bench.transcript
    ./p03 audit --transcript bench.transcript

## The ordering, and the two places it lives

The chapter's rule is that the GNSS receiver is powered down before the radio
transmits. This program holds to it in two different ways, and they answer
different questions.

**Enforced.** Every transmit goes through one function, and that function
refuses while the program's model of the receiver says it is powered. It does
not log a warning and send the command anyway: the command is never written.
That makes the rule true of this program.

**Audited.** `audit` reads a recorded session and answers the same question
about it. That matters because the enforcement protects this program and
nothing else. A bench session typed by hand in `minicom`, or a script somebody
wrote in an afternoon, has no such function in it, and all it leaves behind is
a transcript. `audit` works on any transcript, including ones this program
could not have produced, and the `gnss-left-on` scene exists so the audit has
a known positive to find. A sweep that has never found anything has not been
shown to be able to.

## Three things the program refuses to do

**Publish over the wrong bearer.** `AT+CNMP=38` and `AT+CMNB=1` are a request.
`AT+CPSI?` is the answer, and a module on NB-IoT or on a GPRS fallback will
answer every other command in this lab perfectly happily and publish a payload
that looks right. Bring-up stops instead.

**Substitute the host clock.** The stamp in the payload is the receiver's.
A cycle with no new fix republishes the cached one with the stamp it came
with, so a receiver sees the same stamp twice and knows the cycle produced
nothing new. A cycle with no fix and nothing cached is skipped. Filling it in
from the Pi's clock would destroy the one property that makes a repeat
visible, which is the whole argument of the chapter's rule.

**Believe a fix that is not there.** `+CGNSINF: 1,0,,,,` is the normal answer
from a receiver powered four seconds ago. The run status is 1 because the
receiver is on, and that says nothing about whether it has found anything.
A fix needs the fix flag, a stamp and both coordinates, and anything less
yields no fix rather than one made of empty strings.

## What each file is

| File | What it holds |
| --- | --- |
| `src/p03.h` | every declaration, and the rules above as a comment |
| `src/fix.c` | the fix, the payload and the bearer, each refusing rather than filling in |
| `src/session.c` | the cycle, and the one door everything transmits through |
| `src/scene.c` | transcripts of a module that is not here, including one that breaks the rule |
| `src/audit.c` | the same ordering question, asked of a recording |
| `src/main.c` | four subcommands and the exit status rules |
| `tests/test_p03.c` | the suite, five groups, no framework |
| `../lib/atchat.h` | the AT engine, shared with P02 |
| `../lib/linereader.h` | the line reader the audit uses, shared with P06 and P18 |

## A note on AT+SMPUB

This program sends the four argument form, topic, content length, quality of
service and retain, and then the payload followed by Ctrl-Z. The chapter's
earlier draft showed three arguments. Confirm the order against the SIMCom AT
manual for your firmware revision before relying on either: the argument list
of this command is a module convention rather than a standard, and it is the
kind of thing a revision changes.

## Exit status

Zero means the question was answered. Two means the command line was wrong.
One means either the question could not be answered, with the reason on
standard error, or a gate was applied and failed. The two gates are the
bearer, which stops a session, and the audit, which fails a recording in
which anything transmitted while the receiver was powered.
