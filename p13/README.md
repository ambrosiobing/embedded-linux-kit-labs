# P13: the supervisor of the dual-radio signalling failover

The Linux side of lab P13, in C. Two states, one timer, one input.

**What it does.** Reads tagged health records on standard input, keeps the
time of the last primary record, falls to FAILOVER when that time is older
than the window, and returns to PRIMARY on the next primary record. Writes a
log of what it saw, replays that log, and judges it against the chapter's
acceptance test.

## State, stated plainly

**Compiled and run against generated logs, never run against a broker.** No
MQTT, no radios, no antenna was unscrewed. The window is a number this
program applies; whether 45 seconds is the right number for a real bench is a
question for a working day of observation, which is what the chapter says.

## Build and check

    make            build p13 and the suite
    make check      run the suite: no hardware, no broker, no clock
    make demo       five runs that did not happen, judged
    make strict     a second pass with the conversion warnings turned on

## With no broker

    ./p13 scene --kind outage --out outage.p13
    ./p13 replay --source outage.p13
    ./p13 verify --source outage.p13

    ./p13 scene --kind steady --out steady.p13
    ./p13 verify --source steady.p13      # exits 1, and says why

## On the bench

    mosquitto_sub -h localhost -t lab/p13/health -v | \
        p13 supervise --log bench.p13

    ./p13 verify --source bench.p13

The supervisor reads what `mosquitto_sub -v` prints and nothing else. It does
not open a socket, subscribe to anything or know what a broker is, which is
the same separation the chapter asks of the design: all the intelligence in
one place, and that place small enough to read.

## The clock is an argument, not a call

Nothing in the state machine reads a clock. The time arrives as a parameter,
from `CLOCK_MONOTONIC` during a live run and from a column of a log during a
replay, and the machine cannot tell which. Three things follow.

A replay of a run reaches the same transitions at the same times as the run
did, because the replay also ticks at one hertz between records, which is
what the live loop does.

The window boundary is testable exactly rather than approximately. The suite
asserts that at forty five seconds the primary is late and at forty five
point zero zero one it is missing, which no test with a real clock in it
could do.

A ten minute run is tested in microseconds, so the suite runs five of them
and nobody is tempted to skip it.

## What the program refuses

**A healthy run as evidence of a failover test.** `verify` fails a log that
never fell, in those words. A steady run is a fine thing and it demonstrates
nothing about what happens when an antenna comes off.

**A log judged against the wrong window.** A log records a run at one window
and reading it against another makes the fall look late or early. The suite
checks that too, by judging a 45 second run against a 10 second window and
requiring the refusal.

**A tag it did not read.** The string `cat4` or `nb` is written by the host
that owns the radio. A tag assigned here would describe what this program
believed rather than what arrived, which is the thing under test, so a record
whose source cannot be read is counted as unreadable and acted on no further.

**Silence as good news.** The run starts in FAILOVER, because nothing has
been heard yet, and a supervisor that starts in PRIMARY is claiming a route
it has never seen. The backup speaking never brings the state back either: a
route is not declared good because a different route spoke.

## What each file is

| File | What it holds |
| --- | --- |
| `src/p13.h` | every declaration, and the rules above as a comment |
| `src/machine.c` | the tag reader, the two states, the log |
| `src/replay.c` | re-running a log, and the acceptance test over one |
| `src/scene.c` | runs that did not happen, written by the real machine |
| `src/main.c` | four subcommands and the exit status rules |
| `tests/test_p13.c` | the suite, five groups, no framework |
| `../lib/linereader.h` | the line reader, shared with P03, P06 and P18 |

## The five scenes, and why four of them fail

| Scene | What it is | Verdict |
| --- | --- | --- |
| `outage` | the primary stops for two minutes and returns | passes |
| `steady` | both publishers, no outage | refused: the state never fell |
| `short-gap` | the primary misses one sample | refused: inside the window, so nothing fell |
| `no-return` | the primary stops and never comes back | refused: half the test |
| `backup-only` | the primary is never heard at all | refused: one source is not two |

The `short-gap` scene is the one worth keeping. A window of one sample would
flip on it, and a reader who shortened the window to make the lab more
responsive would find that out in the field rather than here.

## Exit status

Zero means the question was answered. Two means the command line was wrong.
One means either the question could not be answered, with the reason on
standard error, or the acceptance test was applied and failed. `supervise`
reports and exits zero whatever the radios did, because an outage is a
result rather than a fault in the program.
