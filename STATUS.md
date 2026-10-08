# What has actually been run

Every lab in this volume ends in an acceptance test that can be failed. Until
this file existed, nothing in the repository said whether any of them had been
passed, which left a reader to assume that twenty written labs were twenty
working ones. They are not, and the gap is the point of this page.

A lab sits at the highest state it has **fully** earned. Partial credit is
recorded in the last column rather than in the state, because a state that can
be half held is a state that means nothing.

| State | What it means |
| --- | --- |
| **written** | written against the inventory, and audited for header width, logic voltage and exclusive claims |
| **checked** | its commands or its program are syntax-checked, compiled or run on synthetic data, with no hardware anywhere |
| **run** | run on the named host, with the board revision written down |
| **passed** | every item of its acceptance test is satisfied and the numbers are recorded, with the date, the OS image and the board revision |

## The twenty labs

| Lab | State | Evidence | What the next state needs |
| --- | --- | --- | --- |
| P01 | written | the pairing audit | the SIM7600E-H seated on the Pi 4, and a SIM |
| P02 | written | the pairing audit | the SIM7020E seated on the Pi 3, and an NB-IoT SIM |
| P03 | written | the pairing audit | the SIM7070G seated on the Pi 3B+, and a Cat-M SIM |
| P04 | **checked** | compiled with warnings as errors and run end to end on synthetic data by this repository's own checks, on every push | the STWIN.box on the Pi 4's USB, and a real capture through the same path |
| P05 | written | the pairing audit | the MCC 118 seated on the Pi 4. See the note below: this lab cannot reach **passed** on this bench |
| P06 | written | the pairing audit | the 53L8A1, the LCD and the Nucleo together on the Pi 3 |
| P07 | written | the pairing audit | the IKS4A1 stacked on the Nucleo |
| P08 | written | the pairing audit | the IKS5A1 stacked on the Nucleo |
| P09 | written | the pairing audit | the NanoPi NEO Air brought up headless with the ADXL345 |
| P10 | written | the pairing audit | the PPK2 on the ESP32. The PPK2 has been used on this bench, but for P15's modules, not for this lab's target |
| P11 | written | the pairing audit | the Explorer700 seated on the Pi 3B+ |
| P12 | written | the pairing audit | the official display and keyboard on the Pi 4, and a broker |
| P13 | written | the pairing audit | two Pis and two modems at once, which is the largest setup in the volume |
| P14 | written | the pairing audit | the Renkforce cable and two ESP boards on the Pi 3 |
| P15 | **run** | three LK-LED10 modules driven from header pins 11, 13 and 15 on a Raspberry Pi 3 Model B Rev 1.2, Friday 2 October 2026; each module's current measured alone on the PPK2 at 5.000 V and 3.300 V, Saturday 3 October 2026 | the rail voltages, which need a voltmeter this bench does not have, and a written lab book |
| P16 | written | the pairing audit | the ESP32 on the Pi 4 |
| P17 | written | the pairing audit | the ESP8266 on the NanoPi |
| P18 | written | the pairing audit | a bare Nucleo on the Pi 4 over USB |
| P19 | written | the pairing audit | **one part, no longer one question.** The carrier schematic settled on Thursday 8 October 2026 that the board covers header pins 27 to 40, so the free bit-banged route is closed. The lab needs a 2x20 stacking header, or a tall header whose pins 27 to 40 clear the 26-pin socket, and this bin has neither |
| P20 | written | the pairing audit | the other nineteen, or at least four hosts reachable at once |

## Two things this table says that are worth reading twice

**P05 cannot reach passed on this bench, and no amount of bench time will
change that.** Its acceptance test asks that the multimeter reading of the
3.3 V rail and the CH0 column of the capture agree, and that both are written
down. There is no voltmeter here. The MCC 118 itself cannot stand in, because
the whole point of that item is an independent instrument disagreeing with the
one under test. So P05 can reach **run**, and its first three acceptance items
can be satisfied, but the fourth state is bought with a multimeter and nothing
else. That makes a cheap multimeter the single purchase that unblocks the most
acceptance tests in this volume, ahead of the stacking header P19 may need.

**P15 is the lab closest to a recorded pass, not P05.** Three of its seven
acceptance items are already satisfied and written into the chapter: the common
ground was shown by a module lighting from a host pin, one module's polarity
was settled by three controls in the order high, low, released, and no jumper
ran between the 5 V rail and any header pin at any point. Two more are within
reach in one sitting, since the semaphore item asks for a program rather than
the three `pinctrl` commands that have stood in for one so far. Only the rails
item needs the instrument this bench lacks.

## The build beside P04 that failed on every run, read on Tuesday 6 October 2026

P04 is at **checked** on the strength of a build with `-Werror` and a suite that
runs on synthetic data. Beside it sits a second build that had never passed.

The `strict` target rebuilds the same sources with `-Wconversion`,
`-Wsign-conversion` and `-Wdouble-promotion` added on top of that same
`-Werror`. It exited 2 on every run. The workflow marks that step
`continue-on-error: true`, which was deliberate and is not revisited here: the
step is named "reported but not gating" and it did exactly that.

What was not deliberate is that nobody read the report. A step marked
`continue-on-error` is recorded as **success** when it fails, so each of those
runs showed fifteen green steps, and the failure survived only as an annotation
counted in a summary line. It sat there through three runs across Sunday 4
October 2026 and Monday 5 October 2026, read as noise every time.

Built on win11 skyhorizon's WSL, it turned out to be four findings, all of one
kind, all on two adjacent lines:

```
tests/test_p04.c:202:38  implicit conversion from float to double
tests/test_p04.c:202:62  to match other operand of binary expression
tests/test_p04.c:203:38  [-Werror=double-promotion]
tests/test_p04.c:203:62
```

Both lines compute a magnitude as `sqrt((double)re[8] * re[8] + ...)`. Only the
first operand of each multiply carries the cast, so the second is promoted
implicitly to match it. Casting both operands fixes all four and changes no
arithmetic: the products were always computed in double, and now the source
says so instead of leaving it to the promotion rules.

Two things worth keeping from that. **Nothing in `src/` was flagged at all**, so
the library code is already clean under `-Wconversion` and `-Wsign-conversion`,
which is the pair that matters for a lab whose discipline is samples staying
`int16_t` until the one place they become floats. And the failure was never
subtle or large. It was four casts, invisible for three runs purely because the
step that found them was allowed to pass.

## What is missing from the evidence column, and should not be next time

Neither bench sitting recorded the OS image. The board revision is there,
`Raspberry Pi 3 Model B Rev 1.2` from `/proc/device-tree/model`, and so is the
userspace, libgpiod v2.2.1 over `gpiochip0` on `pinctrl-bcm2835`, but the image
date and the kernel are not. A state of **passed** requires them, because
"Bookworm" alone will not tell the next reader what was running.

**Four commands, in this order, and their output belongs in the lab book beside
the numbers.** This block is shared with the sibling bench volume, so that two
books by the same author stop asking different questions about the same board.

```
cat /etc/os-release
uname -a
cat /proc/device-tree/model
findmnt /
```

They answer four separate questions and none of them answers another's.

| Command | Question | Why this one |
|---|---|---|
| `cat /etc/os-release` | which distribution | Operating system identification data, with `NAME`, `ID`, `VERSION_ID` and `PRETTY_NAME` set by the vendor. It says nothing about the kernel. See https://man7.org/linux/man-pages/man5/os-release.5.html |
| `uname -a` | which kernel | Prints sysname, nodename, release, version and machine on one line, so it includes the release that `uname -r` would print alone, and keeps the version field. See https://pubs.opengroup.org/onlinepubs/9699919799/utilities/uname.html |
| `cat /proc/device-tree/model` | which board | The string this bench recorded as `Raspberry Pi 3 Model B Rev 1.2` |
| `findmnt /` | which root filesystem | Searches the kernel mount table, by default `/proc/self/mountinfo`, and with a mountpoint argument shows what is mounted there, source included. See https://man7.org/linux/man-pages/man8/findmnt.8.html |

**`findmnt /` is the one this volume was missing, and it catches a specific
failure.** A board that boots is not evidence that the image you built is the one
running. The sibling volume's NanoPi arrived with a vendor image on its eMMC, so
a successful boot there proves only that *something* booted. The same risk
applies to any host in this bin that has ever had another card in it.

**Why `uname -a` rather than `uname -r`.** The sibling volume asked for `-r`.
The release field that `-r` prints is still readable inside `-a`, so nothing is
lost, and what `-r` drops is the version field. That field is already
load-bearing in the sibling volume, where the preemption model is decided by
looking for `PREEMPT_RT` in `uname -v`. One command answers both questions, and
keeping two spellings of "which kernel" would preserve exactly the disagreement
this block exists to remove.

**One footnote, and deliberately not a fifth command.** `findmnt` is part of
util-linux and is present on the Debian bookworm rootfs and on Raspberry Pi OS.
If some later image turns out to be BusyBox only, the same question is
`awk '$2=="/"' /proc/self/mountinfo`. Do not put that in the block unless an
image actually lacks `findmnt`.
