# P07 and P08: one program, for a reason P01 and P09 do not have

The labs are [consumer 9-DoF plus climate](../chapters/07-consumer-9-dof-plus-climate-iks4a1.md)
and [industrial high-g and dual-scale baro](../chapters/08-industrial-high-g-and-dual-scale-baro-iks5a1.md),
and their shared program was written on **Sunday 19 October 2026**.

This volume already has one shared program. P01 and P09 use `p01/` because
their chapters say the difference between the two publishers is a bus number:
same code, same suite, one flag.

That reason does not apply here. These are different shields with different
parts and different keys, and almost nothing about the two captures is the
same. What makes them one program is a single line in P08's acceptance test:

> The numbers live in a different log file from P07.

An acceptance item in one lab that is about another lab cannot be checked by
a program that knows only one of them. So `p07 pair` is written once, in a
place that knows both, and the directory follows the lower number as `p01/`
does.

## The rule, which is the lab

P08's intent states it in one sentence and the data-flow section draws it as
structure rather than as advice:

> two streams, two keys, two log files. Collapsing them into one acceleration
> field is how a wearable-grade number ends up wearing an industrial label.

And then it names the exact shape of the error: an analysis cannot find the
two streams "already merged in a field called `acc`" and assume the merge was
correct.

So `acc` is refused by name, in all three profiles, and the refusal quotes
the chapter. There was no judgement to make about what the forbidden key is
called, because the chapter had already made it.

## Nothing here is an invented format, and one rule had no name

This was the first lab where the volume's second rule, no invented wire
format, had real force. The firmware owns the record, so the risk was
choosing key names to suit a parser.

There was nothing to choose. P07 prints its own record in its data flow,
`{"t":...,"rh":...,"p":...,"ax":...,"ay":...,"az":...}`, and P08 names its
two files there with their one key each. Every name this program looks for is
quoted, and `p07 profiles` prints the list so a reader can check the quoting
rather than trust it.

**One rule is stated without a name**, and that one took a decision. The
ILPS22QS has two scales, and the chapter says the scale is part of the record
because "a pressure without the range it was measured in is a number with two
possible meanings". No chapter says what the scale key is called. Naming it
here would have been inventing the thing the rule forbids, so the check is on
the rule instead: a record carrying a pressure must carry some key whose name
contains `scale`. Weaker than a name and honest about why.

## Two failures that look alike, and are two pitfalls

A capture can be short for two reasons and the chapters treat them
separately. P07's pitfalls list has both:

> **A schema that drifts.** Adding a key halfway through a capture leaves a
> file that no parser reads end to end.

> **Choosing a sensor-hub or ISPU mode by accident.** Those modes route parts
> through the LSM6, so the firmware scan finds fewer addresses than the
> shield carries. The symptom is a JSON object that silently loses keys.

The second one has a perfectly stable schema. It is short from its first line
to its last, so a stability check alone passes it without comment, and the
capture is still wrong. That is why the required-keys check is a check of its
own rather than a corollary of stability, and why the suite has a scene for
each: `drift` fails only stability, `hubbed` fails only required keys, and
the suite asserts for each that every other check still passed.

The workflow asserts something slightly stronger about the pair: that the
hubbed capture's refusal does **not** mention a drift. Two different pitfalls
that produced the same sentence would be one pitfall with two names.

## The scene that is wrong in two ways, which is not a flaw

Four of the five bad captures fail exactly one check. The fifth, `collapsed`,
fails two: it carries `acc` and it has lost `lowg`.

That is not a defect in the scene. Collapsing two streams into one field is
both of those things at once, and a scene that only added the forbidden key
without removing the required one would describe the mistake as half of
itself. The suite asserts both halves and says why.

## The parser, and what it does not reach

It extracts the top-level keys of one JSON object without a library, because
the values are not needed and this volume does not allocate on a data path.
That is still a parser, and the three ways a naive one goes wrong are each
handled and each tested: a brace inside a string is not a brace, a key inside
a nested object is not a key of the record, and a line that is not a JSON
object is refused whole rather than half read.

What it does not reach is written in the source rather than left to be found.
A string escape is handled only far enough to keep the parser in step, so a
key written `"a\nb"` reads as `anb`. No key in either chapter has an escape
in it, and a key that needs one is worth objecting to for other reasons.

**Key sets compare as sets, not as ordered lists**, and that was a decision
rather than an oversight. The chapter's rule is "the same keys from the first
line to the last", and a reader of a `.jsonl` parses by name. Two records
carrying the same names in a different order carry the same schema. Comparing
order would be a stricter test of the firmware and a weaker test of the rule,
and the rule is what the acceptance item is about.

## The first build, and two refusals that pointed at the wrong thing

Built in WSL on win11 skyhorizon on **Sunday 19 October 2026**: no
diagnostics, 61 checks with none failed, and the stricter pass clean on its
first attempt, which is now four labs running.

The demo found two things and neither is a wrong verdict. Both are the part
of the output that merely explains, which is the part nothing asserts
against, and it is the second day running that is where the defects were.

**The collapsed capture was sent to the wrong end of the bench.** Its
refusal for the missing `lowg` carried the sensor-hub explanation: that an
ISPU mode routes parts through the LSM6, so the scan finds fewer addresses
and the record loses keys. True of a hubbed capture and false here, where the
cause is printed in the very next line: the key was renamed into `acc`. A
reader following that paragraph would go and read firmware modes while the
answer sat on screen. Which cause to name now depends on whether anything
forbidden was found in the same file.

**And the closing paragraph talked about humidity to a shield that has
none.** It named breathing on the SHT40 and a palm over the STTS22H under all
three profiles, and P08's chapter states plainly that its shield has no
humidity part. That text belongs to the profile rather than to the report,
and it now sits there, with P08's own items in it: the drop, the two channels
in their own keys, and the quiet-room noise in pascals.

## The heredoc, for the third time

Writing the second fix through a shell heredoc turned the escaped newlines
inside two C string constants into real ones, leaving `log.c` with
unterminated literals. That is the trap this catalogue already has a note
about, and the note says to use the editor instead.

It cost nothing this time because the damage is a compile error. What it did
produce is a small checker worth keeping: a sweep for a string that opens on
a line and does not close on it, which is exactly the shape that damage
takes, and which can run on the laptop that is not allowed to compile. Its
first version reported nine lines in files that were fine, because it did not
understand a character literal holding a quote or a quote escaped inside a
string. A guard that cries wolf is worse than none, since the next real
finding reads as the tenth false one. The corrected one reads 91 files clean
and was then proved on a planted decoy of the exact damage shape.

## What stays a claim, as of Sunday 19 October 2026

**It builds and its suite passes, and the two report fixes have not been
rebuilt.** No shield has been stacked, no Nucleo flashed, and every capture
this program has read is one its own `scene` wrote.

`STATUS.md` leaves both labs at **written** for one more round rather than
taking the 61-check run as the evidence it looks like, because that run was
of the version that sent a reader to the wrong cause. Both rows move to
**checked** when 61 come back clean on the fixed code.
