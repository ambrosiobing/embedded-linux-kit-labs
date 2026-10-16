# P02: the host half of the NB-IoT power-saving field node

The Linux side of lab P02, in C, written and tested before the HAT is seated.
The lab is the duty cycle rather than the payload, so most of this program is
about two eight-character bitmaps and what they add up to.

**What it does.** Decodes the `AT+CPSMS` timers into seconds, works out what a
send period and an active window come to over an hour, refuses a schedule that
never lets the radio sleep, and drives the whole AT conversation as a state
machine that waits for each command's final line.

## State, stated plainly

**Compiled and run against transcripts, never run against a module.** No SIM,
no antenna, no network. The timer tables are read from the specification and
nothing here has confirmed them against an operator, so what this program
prints is the meaning of a request rather than a measurement of a grant.

## Build and check

    make            build p02 and the suite
    make check      run the suite: no hardware, no serial port, no SIM
    make demo       the four scenes, replayed
    make strict     a second pass with the conversion warnings turned on

## With no module

    ./p02 timers --tau 00100011 --active 00000101
    ./p02 plan --tau 00100011 --active 00000101 --period 600
    ./p02 transcript --scene attached --out attached.transcript
    ./p02 session --transcript attached.transcript

## With the HAT

    ./p02 session --port /dev/ttyAMA0 --cycles 6 --endpoint 172.16.0.1 \
        --log bench.transcript

The log is written in the transcript format, so a session with a module can be
replayed afterwards with none:

    ./p02 session --transcript bench.transcript

## The two timers, and why they need a program

`AT+CPSMS=1,,"00100011","00000101"` asks for a long update interval and a
short active window. Both arguments are three bits of unit and five bits of
value, **and the two unit tables are different tables**. The periodic update
timer is GPRS Timer 3 of 3GPP TS 24.008 section 10.5.7.4a, where unit 001
means an hour. The active time is GPRS Timer of section 10.5.7.3, where unit
000 means two seconds. So those two bitmaps are three hours and ten seconds,
and the same eight characters read with the other table would be fifty
minutes.

A host script that writes the bitmap into a command and never decodes it
cannot tell those apart, and both look equally plausible on the glass. This is
the first thing the program does and the first thing the suite checks.

## What the readback does not tell you

`AT+CPSMS?` returns what the module holds as its request. A module that
returns the octets it was given has told you it accepted the request, not that
the network granted it. The grant is a separate thing that arrives from the
network, and the chapter's fallback of cycling the radio by hand exists
because it often does not arrive at all. The session says which of those two
it saw, in those words, rather than printing a match and leaving the reader to
draw the wrong conclusion.

## Three things the program refuses to do

**Run a schedule that never lets the radio sleep.** If the send period sits
inside the active window, the radio is reachable continuously and the lab
measures the opposite of what it claims. `plan` exits non-zero and says so.

**Send a record that outgrew its budget.** The payload is hexadecimal, so the
wire length is half the string, and twenty bytes is the chapter's budget. An
odd number of characters, a character that is not a hexadecimal digit, and a
record over the budget are all refused before the port is opened.

**Guess at a reply.** Every command waits for its own final line: `OK`,
`ERROR`, a `+CME` or `+CMS` error, or a prompt. A deadline that expires is a
timeout and is counted apart from a refusal, because a module that is not
listening and a module that declined are different faults with different
fixes.

## The indicator rule, as a count

The chapter says the indicator follows `AT+CEREG?` and nothing else. The
session decides it from the registration state before the socket is opened and
does not revisit it, so a refused datagram leaves it on. The suite asserts
exactly that: a cycle whose send is refused still counts one indicator cycle,
because the question that indicator answers is whether the network knows this
module exists.

## What each file is

| File | What it holds |
| --- | --- |
| `src/p02.h` | every declaration, and the rules above as a comment |
| `src/timers.c` | the two unit tables, the duty cycle, the payload budget |
| `src/session.c` | the conversation, and the indicator rule |
| `src/scene.c` | transcripts of modules that do not exist |
| `src/main.c` | four subcommands and the exit status rules |
| `tests/test_p02.c` | the suite, six groups, no framework |
| `../lib/atchat.h` | the AT engine, shared with P03 |

## Exit status

Zero means the question was answered. Two means the command line was wrong.
One means either the question could not be answered, with the reason on
standard error, or a gate was applied and failed. A session that finds the
module unregistered for every cycle still exits zero: an unregistered wake is
a result, and the count of them is part of what the lab reports.
