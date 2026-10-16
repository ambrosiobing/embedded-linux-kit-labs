# P05: three outcomes where a gate usually has two

The lab is [MCC 118 100 kS/s voltage recorder](../chapters/05-mcc-118-100-ks-s-voltage-recorder.md),
and its file half was written on **Saturday 17 October 2026**. The lab's
earlier record, [p05.md](p05.md), is about an acceptance band that turned out
to be fair by luck. This one is about the item that cannot be reached at all.

## The program that was not written

The obvious thing to write for this lab is the recorder, and it is the one
thing here that should not exist.

The acquisition is four calls into MCC's own library. That library holds the
factory calibration read out of the board's EEPROM, which is the entire
reason this is the only calibrated analog path in the kit. A thin wrapper
around somebody else's calibrated driver adds a layer that can be wrong
without adding a layer that can be right, and it would put this volume's name
on numbers whose correctness belongs to a vendor.

So the chapter prints the four calls, with the vendor's own names and
argument order taken from their C examples, and this program starts where
their file does. The volume already has that rule for a vendor-owned frame
format, in P04, where the chapter says not to invent the probe's framing. A
vendor-owned driver is the same rule one layer up.

One thing was added to the chapter's version of those calls that its earlier
draft did not have: the status word. `mcc118_a_in_scan_read` returns one, and
two of its bits say that the board outran the host or that the host did not
empty the buffer. A host-side wrapper usually drops it. A scan that overran
is a scan with a hole in it, and those two bits say which end of the cable
lost the race.

## Three outcomes, because two would hide the point

`STATUS.md` has said for two weeks that P05 cannot reach a pass on this
bench. Its eighth acceptance item asks that a multimeter reading of the
3.3 V rail agree with the CH0 column of the capture, and that both be written
down. There is no voltmeter here. The MCC 118 cannot stand in, because the
whole point of that item is an independent instrument, and a board checking
itself is not one.

A gate with two outcomes has to call that a failure, which is wrong in a way
that matters. The lab is not failing. It is finished to the edge of what this
room contains.

So every item lands in one of four marks, and the three that are not a pass
mean different things:

| Mark | What it means | What fixes it |
| --- | --- | --- |
| `pass` | measured, or recorded, and right | nothing |
| `FAIL` | measured or recorded, and wrong | the bench |
| `none` | nobody wrote it down | an afternoon |
| `n/a ` | no instrument in this room can settle it | a purchase |

And `verify` has a third exit status, 3, for the case where every item this
bench can settle passed and one could not be settled here. The suite asserts
the whole argument, including its last line: the same capture and the same
book with one multimeter reading added passes on every count. That is the
sum of what that purchase buys, stated as a test rather than as an opinion.

## Absent is not false

Every field of the lab book has an absent state and absent is never read as
false. A rail nobody measured and a rail that measured wrong are different
facts, and a report that printed both as a failure would hide the only one of
them a walk back to the bench can fix.

The awkward case is in the suite: **a recorded reading of zero volts is a
reading.** A book that stored it as a missing field would turn a measurement
of a dead rail into a measurement nobody took, which is the most misleading
single thing this format could do.

## What a row count does not catch

The chapter says to check the file before looking at the picture, because a
plot hides a hole and a row count does not. Writing the checker turned up
three more things a row count misses, and each is now a file the suite reads:

**An index written twice** gives a file with *more* rows than a whole one. A
row count calls it complete. The index does not.

**A file that stopped early** has no holes, nothing out of order and starts
at zero. It is whole in every sense except the one the lab asked for, which
is why the sample count is a separate acceptance item from the holes and not
a restatement of it.

**A sample past the 10.1 V input limit** is a wiring question arriving
disguised as a data question, and it is counted on its own.

A row that does not parse is counted and dropped, never read as a zero, and
the index it was carrying is then reported as a hole, which is what it is.

## What is only a claim, as of Saturday 17 October 2026

**None of this has been compiled**, and `STATUS.md` leaves P05 at
**written**.

Two things will stay claims after the first green run. No voltage in any test
here came from anything but arithmetic, so what the suite proves is that the
program agrees with its own reading of a file. And the four vendor calls in
the chapter are quoted from MCC's published C examples rather than compiled
against their library, which is not on this machine: the names and the
argument order are theirs, read from their source, and the first thing the
board will test is whether that reading was right.
