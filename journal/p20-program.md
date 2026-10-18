# P20: the lab whose program is deliberately not its deliverable

The lab is [four-host fleet health](../chapters/20-four-host-fleet-health.md)
and its program was written on **Sunday 18 October 2026**. It is the last
chapter in the volume and the first one where the right answer was to leave
the deliverable alone.

## The sweep stays shell

Every other lab here got a program because the lab's product is one. This
lab's product is a forty-line shell script and the text file it appends to,
and the chapter says so plainly: no dashboard, no database, no alerting, and
when the acceptance test is run it is run against that file.

A loop over four SSH sessions is what shell is for, and reimplementing it in
C would have produced a worse version of a thing that already works. So
`p20/` never opens a session, never asks a host anything, and cannot repair
anything. It reads the artifact afterwards and decides whether the artifact
is what the chapter says it is.

That felt like a smaller lab until it was written, and it is not. Four of the
chapter's seven pitfalls are decidable from the file alone, and the fifth
thing it does is the one worth the typing.

## Two kinds of absence that look identical

`vcgencmd` is a Raspberry Pi tool and the NanoPi has none. Its header bus is
0 rather than 1, which P09 established on the bench. The chapter's sweep
discards the error output on purpose so that both show up as a shorter block,
and the chapter says a shorter block is a true statement about that host.

**In the file, that looks exactly like a command that failed on a Pi.** Same
missing line, same shape, two completely different facts. One needs nothing
done about it and the other is a bench problem.

So the program carries which hosts answer what, and counts the two
separately: an honest absence under `air`, a gap under `pi3bp`. A single
"fields missing: 2" would be a number nobody could act on, which is the kind
of summary that reads as rigour and is worse than silence.

The suite has one fleet file where both happen at once, because that is the
only way to show the two counters are not the same counter. A test that only
ever saw one kind at a time would pass with the two merged.

## Each bad scene is wrong in exactly one way, and that is asserted

There are five failing scenes and one good one. For each failing scene the
suite asserts the matching check failed **and that every other check still
passed**.

That is the part that makes five checks five checks. A set of checks that all
fail together is one check with five names, and the output would still look
thorough. It also catches the real risk in a parser like this: that one
malformed line derails the block tracking and everything after it reads as
broken, which would make every scene fail every check and look like a very
strict program.

## The ordering the parser depends on, which has a trap in it

The chapter's sweep writes each host's `hostname` as the first line of its
block, so a line that is exactly one of the four fleet names opens a block.
No format is invented: that is what the chapter's own commands already print.

But `vcgencmd` prints `temp=46.2'C`, which is a bare token with no space in
it, and a parser that looked for host names first would read a temperature as
a fifth host and then refuse the sweep for having one. So the lines with a
shape of their own are matched first and the bare-token test comes last. The
suite checks it directly, with a file containing nothing but a temperature.

## One word added to the chapter

The sweep now labels its HAT line: `hat: none` rather than bare `none`.

The chapter already insisted that an empty header report `none` explicitly,
because a blank reads as a broken script rather than as an empty header. An
unlabelled product string has the same problem one step further on: in a file
of `uptime`, `free` and `lsusb` output, a bare board name is
indistinguishable from any other line. The label makes `hat: none` and
`hat: MCC 118` both reports, and a missing line not one.

That is a change to the chapter's own command, so it went into the source and
is dated in the prose rather than slipped in. Nothing else about the sweep
moved.

## A field that counted the wrong thing

The first version counted every `hat:` line toward the exclusivity check, so
a host that reported an empty header twice would have been refused for having
two HATs on one header. Two reports of nothing seated is untidy and is not a
violation of the rule. It counts named boards now, and the field is called
what it counts.

Caught by reading rather than by the compiler or the suite, which is worth
noting because the suite would not have caught it: no scene writes `hat: none`
twice, and adding one to catch it would have been adding a test for a case
nobody had thought of yet.

## The first build, and one line that read wrong

Built in WSL on win11 skyhorizon on **Sunday 18 October 2026**: no
diagnostics, 51 checks with none failed, and the stricter pass clean on its
first attempt, which is now three labs running.

The demo found one thing, and it is a report rather than a defect. The
two-HAT sweep printed `HAT SIM7600E-H, and 2 HATs named`, and SIM7600E-H is
simply the second of the two, because only the last name is kept. Printing
one of two as though it were the one that is seated is the wrong half of a
true statement. It now says `2 HATs named, the last of them SIM7600E-H`,
which is what the program actually knows.

Worth noting what did not happen: the refusal itself was right, named the
host, and sent the reader to the rule. The line that was wrong was the part
that merely describes, which is the part nothing asserts against.

## What stays a claim, as of Sunday 18 October 2026

**It builds, both passes are clean, and no host has been swept.** No SSH key
has been copied and every fleet file this program has read is one its own
`scene` wrote. `STATUS.md` moves P20 to **checked**, which is what a
compiled program with a green suite and no hardware earns here.
