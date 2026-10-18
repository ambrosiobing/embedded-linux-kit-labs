# P20: the fleet sweep's judge

The host side of lab P20, in C. It does not run the sweep.

**What it does.** Reads a `/var/log/lab-fleet.txt` written by the chapter's
shell sweep and decides whether that file is the acceptance artifact the
chapter says it is.

## Why this lab's program is not the sweep

Every other lab here has a program because the lab's product is one. This
lab's product is a forty-line shell script and the text file it appends to,
and the chapter is explicit about it: no dashboard, no database, no alerting,
and when the acceptance test is run it is run against that file.

So the script stays shell, which is the right answer for a loop over four SSH
sessions, and the program is the thing that reads the artifact afterwards. It
opens no session, asks no host anything, and cannot repair anything.

## The one piece of judgement

`vcgencmd` is a Raspberry Pi tool and the NanoPi has none. Its header bus is
0 rather than 1, as P09 established on the bench. The chapter's sweep
discards the error output so that both show up as a shorter block, and says
that a shorter block is a true statement about that host.

**In the file, that looks exactly like a command that failed on a Pi.** This
program is what knows the difference. The same missing line is an honest
absence under `air` and a gap under `pi3bp`, and they are counted separately
rather than summed into one number nobody can act on. A run where both happen
at once is in the suite, because that is the only way to show the two
counters are not the same counter.

## What it refuses

Four of the chapter's seven pitfalls are decidable from the file alone.

- **A blank where a HAT should be named.** An empty header reports `none`. A
  missing line reads as a broken script, and the two must not look the same.
- **A fifth name in the loop.** The fleet is four hosts; a fifth is a host
  this book has not brought up.
- **Two HATs on one host.** One board owns the 40-pin header, which is the
  rule every lab in this volume is written against. Counted on named boards,
  because two reports of an empty header are untidy rather than a violation.
- **An undated sweep.** A sweep taken in the middle of a teardown describes a
  bench that no longer exists, and without a timestamp there is no way to
  tell which it was.

## The one thing the sweep was asked to add

A label on the HAT line. The chapter already insisted an empty header report
`none` explicitly rather than leaving a blank; an unlabelled product string
has the same problem one step further on, because it is indistinguishable
from any other line of output. `hat: none` and `hat: MCC 118` are both
reports. A missing line is not.

Nothing else about the sweep changed. The block boundary is the `hostname`
line it already prints, and the temperature, the memory and the USB lines are
read in the shape their own commands emit.

## State, stated plainly

**Written Sunday 18 October 2026, and not compiled yet.** No host has been
swept, no SSH key copied, and every fleet file this program has read is one
its own `scene` wrote.

## Build and check

    make            build p20 and the suite
    make check      run the suite: no SSH, no hosts, no sweep
    make demo       one sweep that is an artifact, and five that are not
    make strict     a second pass with the conversion warnings turned on

## With no fleet

    ./p20 scene --kind good --out good.txt
    ./p20 judge --source good.txt

The five other kinds are each wrong in exactly one way: `blank-hat`, `fifth`,
`two-hats`, `undated` and `pi-no-temp`. That matters for the suite, which
asserts for each of them that the matching check failed **and that every
other check still passed**. A set of checks that all fail together is one
check with five names.

## On the bench

    ./p20 judge --source /var/log/lab-fleet.txt

## What this program does not decide

Whether the file matches the bench. Four names and a reported HAT each is
what a program can see. Whether the board it names is the board on the table
is the question the chapter asks a person to answer, and no amount of reading
the artifact settles it.
