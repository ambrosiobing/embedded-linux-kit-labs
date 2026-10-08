# The checks, and three false passes in three days

This file belongs to no lab. It is the record of the machinery that decides
whether the volume is telling the truth, and of the week in which that machinery
repeatedly told the truth about everything except itself.

## What was added, and why each one exists

| What | The failure it exists to prevent |
|---|---|
| `STATUS.md` | Twenty finished-looking chapters, with nothing saying that nineteen had never been run |
| `inventory.json` and its suite | Four rules about parts, headers and voltages that thirteen labs depend on, written as prose that could refuse nothing |
| `PITFALLS.md` | 193 pitfalls readable only inside the lab that owned them, which is the wrong direction for the way the section is used |
| `checkcode.py` | 13 Python and 42 shell blocks printed as instructions to type, never once parsed |
| `BOARDS.md` | Facts about boards, each stated only where it was used, so checking a pin meant reading a lab you were not building |
| `prepublish.py` | Four checks that lost their input when the sources stopped being published |

## Three false passes, and they are the point of this file

Each one produced a result that looked like success, and each was produced by
something other than the thing being tested. Finding three in three days is not a
run of bad luck; it is what happens when you start checking whether your checks
can fail.

**One. A negative test that passed because its file could not be written.** The
test for the status-ladder check mutated a copy and expected a failure. The
temporary directory variable was empty, so the path resolved to the filesystem
root and the write was refused. Every lookup then failed for a missing file and
the test exited with the expected code **for entirely the wrong reason**. Rerun
against a real path, it caught exactly the two injected faults and stayed silent
on the other eighteen rows.

**Two. A drift test that wiped its own evidence.** The check that proves the
generated files are current was tested by editing one and then regenerating,
which overwrote the edit. The diff was clean and the test reported that it had
missed the fault. The real case is a stale **committed** copy, so the second
attempt staged the broken version first, and the check fired.

**Three. A sweep whose fallback printed a reassurance.** A search for forbidden
dashes used a regular-expression mode that failed on this machine's locale, and
the `|| echo "none"` beside it printed "none". Redone in Python, it found no
dashes and one pre-existing character that was fine. The answer was right; the
method had stopped working and said so in the voice of a pass.

All three share their shape with the finding in [p04](p04.md), where a failing
step was recorded as a success for three runs. **A result that looks like success
and was produced by something else is the single most common way a check stops
existing.**

## A drift hole that had been open from the start

While widening the generated-file check, `CONTENTS.md` turned out to have been
generated at the repository root all along with nothing checking it. It could
have gone stale silently for any length of time and nobody would have seen it.

The check now covers the chapters, `CONTENTS.md` and `PITFALLS.md` together, and
it was tested by staging a deliberately stale copy.

## The guard that worked on its author

The first draft of the volume's safety page reached for one of the violent verbs
the house rules forbid, in a sentence about what can happen to a part. The linter
refused it.

That is the only real test of a house rule. A rule enforced against an imagined
future contributor and never against the person writing is a preference with
good manners.

## Thursday 8 October 2026: the sources left, and four checks moved

The authoring sources stopped being published. A chapter is a Markdown file, and
the LaTeX that generates it is not what a reader wants, so 105 source files left
the published tree.

The honest cost is that **the published repository can no longer verify its own
Markdown against a source it does not contain.** Four checks lost their input:
the house rules, the command-block parser, the per-lab figure check, and the
regeneration check. They moved into `prepublish.py`, which runs before a push,
regenerates first because a stale chapter is the failure this volume actually
had, and refuses rather than warns.

**One gap is named rather than hidden.** The 42 shell blocks were checked by
continuous integration, which had a shell linter available. It can no longer see
them, and the authoring machine does not have that linter, so nothing checks them
unless the script is run somewhere that does. It prints a warning saying exactly
that instead of exiting quietly green.

Continuous integration gained one check it can still make: no source file in the
published tree. A rule with no enforcement is a habit, and this volume has
preferred guards to habits since the publishing rule itself became one.

## The one sentence worth taking from all of it

Every check in this volume was written because something had already gone wrong
without one. None of them was foresight. The useful question is not whether your
checks pass, but whether you have ever watched one fail on purpose.
