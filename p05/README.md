# P05: the file half of the calibrated voltage recorder

This is the only lab in the volume that cannot reach a pass on this bench,
and this program exists to say so precisely rather than in a paragraph
somebody has to read.

**What it does.** Reads the CSV the board produced and says whether it is a
recording or something that looks like one. Writes and reads the lab book of
what the operator observed. Judges the chapter's eight acceptance items and
keeps three outcomes apart that are usually run together.

## What this program is not

**The recorder.** The acquisition is four calls into the vendor's library,
that library owns the factory calibration held in the board's EEPROM, and
nothing here reimplements or wraps it. The chapter prints those four calls,
from MCC's own C examples, and this program starts where their file does.

Writing a thin C wrapper around somebody else's calibrated driver would add
a layer that can be wrong without adding anything that can be right. The
volume's rule for a vendor-owned format applies to a vendor-owned driver
too: say so, and stop.

## State, stated plainly

**Compiled and run on files this program generated, never run against a
board.** No MCC 118 has been seated, no rail has been measured, and no
voltage in any test here came from anything but arithmetic.

## Build and check

    make            build p05 and the suite
    make check      run the suite: no hardware, no board, no voltmeter
    make demo       six files the board did not produce, judged
    make strict     a second pass with the conversion warnings turned on

## With no board

    ./p05 scene --kind good --out good.csv
    ./p05 check --source good.csv

    ./p05 book --out bench.p05book --address 0 --header-clear \
        --survived-reboot --second-run-clean --date "Saturday 17 October 2026"
    ./p05 verify --source good.csv --book bench.p05book    # exits 3

## Three outcomes, not two

Every acceptance item lands in one of four states, and the three that are not
a pass mean different things:

| Mark | What it means | What fixes it |
| --- | --- | --- |
| `pass` | measured, or recorded, and right | nothing |
| `FAIL` | measured or recorded, and wrong | the bench |
| `none` | nobody wrote it down | an afternoon |
| `n/a ` | no instrument in this room can settle it | a purchase |

A report that printed the last two the same way would hide the only
distinction that matters here. Seven of the eight items are reachable on this
bench. The eighth asks that an instrument other than the board under test
agree with the board's own CH0 column, and there is no voltmeter in the room.
The MCC 118 cannot stand in for it, because the whole point of that item is
an independent reading, and a board checking itself is not one.

So `verify` has a third exit status. Zero when every item passed, **three**
when every item this bench can settle passed and one cannot be settled here,
and one when something was measured and disagreed. The suite asserts all
three, including the last line of the argument: the same capture and the same
book with one multimeter reading added passes on every count.

## What a row count does not catch

The chapter says to check the file before looking at the picture, because a
plot hides a hole and a row count does not. A row count alone misses three
more things, and the suite has a file for each:

**An index written twice.** That file has *more* rows than a whole one, so a
row count reads it as complete.

**A file that stopped early.** No holes, nothing out of order, and it is
whole in every sense except the one the lab asked for, which is why the count
is a separate item from the holes.

**A sample past the input limit.** Never exceed plus or minus 10.1 V on an
input. A sample beyond that is a wiring question arriving disguised as a data
question, and it is counted separately from everything else.

A row that does not parse is counted and dropped, never read as a zero, and
the index it was carrying is then reported as a hole.

## What each file is

| File | What it holds |
| --- | --- |
| `src/p05.h` | every declaration, and the three outcomes as a comment |
| `src/scan.c` | the file, and the four ways it can look finished and not be |
| `src/book.c` | the lab book, and the eight items judged against it |
| `src/synth.c` | files the board did not produce, in six shapes |
| `src/main.c` | four subcommands and the three exit statuses |
| `tests/test_p05.c` | the suite, four groups, no framework |
| `../lib/linereader.h` | the line reader, shared with P03, P06, P13 and P18 |

## The lab book, and absence

Every field has an absent state, and absent is never read as false. A rail
nobody measured and a rail that measured wrong are different facts. The suite
checks the awkward case too: a recorded reading of zero volts is a reading,
not an absence.

The book also carries the four lines the volume asks for beside any recorded
number, from `cat /etc/os-release`, `uname -a`,
`cat /proc/device-tree/model` and `findmnt /`. A board that boots is not
evidence that the image you built is the one running on it.
