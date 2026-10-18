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
| P01 | **checked** | built with warnings as errors and its 55 checks run on generated captures, Saturday 17 October 2026, gcc under `-Wall -Wextra -Werror` and again under the stricter conversion pass, which was clean. Among those checks: a sensor lying on its side produces nothing at all and free fall produces events, which is the lab's rule about magnitudes rather than axes; and upside down reads minus one gravity whose magnitude is still one, which is the sign extension. The live i2c-dev read is compiled by the same checks and has never had a bus to answer it | the SIM7600E-H seated on the Pi 4, and a SIM |
| P02 | **checked** | built with warnings as errors and its 100 checks run on recorded conversations, Saturday 17 October 2026, gcc under `-Wall -Wextra -Werror` and again under the stricter conversion pass, which was clean. Among those checks: the chapter's own two bitmaps decode to three hours and ten seconds, the same eight characters read with the other timer's table give fifty minutes, and a send period inside the active window is refused rather than reported. The suite and three command-line checks run on every push | the SIM7020E seated on the Pi 3, and an NB-IoT SIM. Whether an operator grants power saving at all is the question this lab exists to ask, and no host code can answer it |
| P03 | **checked** | built with warnings as errors and its 75 checks run on recorded conversations, Saturday 17 October 2026, gcc under `-Wall -Wextra -Werror` and again under the stricter conversion pass, which was clean. The lab's claim is checked twice over: a transmit attempted with the receiver powered sends nothing, shown by the recording not having moved, and a recording in which the receiver was never powered down is refused by the audit. A bearer that is not Cat-M stops the session, and a cycle with no fix publishes nothing rather than a host clock | the SIM7070G seated on the Pi 3B+, and a Cat-M SIM, plus a sky view for the first real fix |
| P04 | **checked** | compiled with warnings as errors and run end to end on synthetic data by this repository's own checks, on every push | the STWIN.box on the Pi 4's USB, and a real capture through the same path |
| P05 | **checked** | built with warnings as errors and its 70 checks run on generated captures, Saturday 17 October 2026, gcc under `-Wall -Wextra -Werror` and again under the stricter conversion pass, which was clean. Among those checks: a file with an index written twice has MORE rows than a whole one and is still refused; a recorded reading of zero volts survives as a reading rather than as an absence; and the best capture and best lab book this room can produce reach seven items of eight. The acquisition is deliberately not here: it is four calls into the vendor's library, which owns the factory calibration | the MCC 118 seated on the Pi 4. See the note below: this lab cannot reach **passed** on this bench, and `p05 verify` exits 3 to say so rather than leaving it to a paragraph |
| P06 | **checked** | built with warnings as errors and its 120 checks run on synthetic frames, Friday 16 October 2026, gcc under `-Wall -Wextra -Werror` and again under the stricter conversion pass, which was clean. Among those checks: the painter writes into a file mapped the way a framebuffer is, with a guard area past the end of the geometry that comes back untouched, and a stream with no frame in it leaves that file byte for byte as it was made. The same build and suite run in this repository's own checks on every push, with two command-line checks beside them: that a baseline is refused from five frames and from a wall with something moving in front of it, and that a hand reads as six zones where an empty field reads as none | the 53L8A1, the LCD and the Nucleo together on the Pi 3, and a hand in a known corner, which is the one claim no suite can settle |
| P07 | written | the pairing audit | the IKS4A1 stacked on the Nucleo |
| P08 | written | the pairing audit | the IKS5A1 stacked on the Nucleo |
| P09 | **checked** | the same build and the same 55 checks as P01, Saturday 17 October 2026, because it is the same program run with `--bus 0`, which is what both chapters say the difference is. The wear arithmetic that belongs to this lab is in those checks: an hour of a surface that never settles is 72000 samples and at most 1800 appends to soldered flash, and the fsyncs-per-event figure stays below one when the commit is on a timer and goes above it when the interval is shorter than the cool-off | the NanoPi NEO Air brought up headless with the ADXL345 |
| P10 | written | the pairing audit. **Its program is in the tree and has never been compiled.** `p10/` reduces a capture to a median and a p95 without holding it, and judges a run against the chapter's own comparisons: three states that are actually distinct, a repeat that reproduced, and the sentence that keeps a dev-board figure from being quoted as a bare-module one. No threshold of its own anywhere, because a state is distinct when the quieter one's p95 sits below the busier one's median | the first compile, then the PPK2 on the ESP32 with its own USB cable out of the bench. The PPK2 has been used here, but for P15's modules, not for this lab's target |
| P11 | written | the pairing audit | the Explorer700 seated on the Pi 3B+ |
| P12 | written | the pairing audit | the official display and keyboard on the Pi 4, and a broker |
| P13 | **checked** | built with warnings as errors and its 70 checks run on generated logs, Saturday 17 October 2026, gcc under `-Wall -Wextra -Werror` and again under the stricter conversion pass, which was clean. Among those checks: the window is tested at its own boundary, at forty five seconds and at forty five point zero zero one; a log of a healthy afternoon is refused because it shows no antenna test; and a log judged against a window it was not run at is refused. The suite and two command-line checks run on every push | two Pis and two modems at once, which is the largest setup in the volume. Whether forty five seconds is the right window is a question for a working day of observation, and the route check belongs to `ip route` on two hosts rather than to this program |
| P14 | written | the pairing audit | the Renkforce cable and two ESP boards on the Pi 3 |
| P15 | **run** | three LK-LED10 modules driven from header pins 11, 13 and 15 on a Raspberry Pi 3 Model B Rev 1.2, Friday 2 October 2026; each module's current measured alone on the PPK2 at 5.000 V and 3.300 V, Saturday 3 October 2026. **The blink program the fourth acceptance item asks for is in the tree and checked**: `p15/`, written Saturday 17 October 2026, built with warnings as errors and its 86 checks run on no chip at all, and again under the stricter conversion pass, which was clean on its first attempt because this program has no floating point in it. Among those checks: the step that lights all three is shown to be the one step a crossed pair of jumpers cannot change, and two of the other four are shown to detect it, which is the whole argument for a program over three commands; and the suite reports which half of `lines.c` it compiled, so a run that silently built the non-Linux fallback cannot read as a pass. Getting there cost two defects, both recorded: a feature macro four other files here already carry, and one of the suite's own checks that asserted a value its author had not worked out. Writing the program found that the chapter quoted a measured current for one triple of modules and attached it to a different triple, which is corrected and dated. **The program building does not move this row**, because the fourth acceptance item wants three modules seen lit at once by somebody in the room | the rail voltages, which need a voltmeter this bench does not have, and one sitting to run `p15 blink` on the board and write the lab book |
| P16 | written | the pairing audit | the ESP32 on the Pi 4 |
| P17 | written | the pairing audit | the ESP8266 on the NanoPi |
| P18 | **checked** | built with warnings as errors and its 94 checks run on synthetic streams, Friday 16 October 2026, gcc under `-Wall -Wextra -Werror` and again under the stricter conversion pass, which was clean on its first run. The same build and suite run in this repository's own checks on every push, with three command-line checks beside them: that the acceptance test refuses a stream missing five samples in two thousand, that a rate nobody measured does not pass as one, and that a line too long for the buffer costs one sample and invents none | a bare Nucleo on the Pi 4 over USB. Nothing else: the recorder is finished, and a character device is one more source |
| P19 | **checked** | built with warnings as errors and its 41 checks run on invented gravities, Saturday 17 October 2026, gcc under `-Wall -Wextra -Werror` and again under the stricter conversion pass, which was clean. Among those checks: a rotation about one axis moves the other angle by exactly nothing, which is the half of the acceptance test a per-axis instrument fails; and the fold at ninety degrees is asserted as the limit it is, so that it is found as a property rather than discovered as a bug. **This is as far as this lab goes**, for the reason in the next column | **one part, no longer one question.** The carrier schematic settled on Thursday 8 October 2026 that the board covers header pins 27 to 40, so the free bit-banged route is closed. The lab needs a 2x20 stacking header, or a tall header whose pins 27 to 40 clear the 26-pin socket, and this bin has neither |
| P20 | written | the pairing audit | the other nineteen, or at least four hosts reachable at once |

## Two things this table says that are worth reading twice

**P05 cannot reach passed on this bench, and no amount of bench time will
change that.** Its acceptance test asks that the multimeter reading of the
3.3 V rail and the CH0 column of the capture agree, and that both are written
down. The MCC 118 cannot stand in, because the whole point of that item is a
second instrument agreeing with the one under test, and no instrument can
witness itself. So P05 can reach **run**, and every item of its acceptance
test but the seventh can be satisfied, while the seventh is bought with a
multimeter and nothing else.

**Two claims in this paragraph are withdrawn, Saturday 17 October 2026.** It
used to say "there is no voltmeter here", and to rank a cheap multimeter as
the single purchase unblocking the most acceptance tests in the volume.

The first is wrong. **The MCC 118 is a voltage-measuring instrument on this
bench**, eight single-ended inputs at plus or minus 10 V, and P05's own first
and sixth items are rail readings taken with it. What this bench lacks is a
*second* instrument, which is a narrower thing, and only P05's seventh item
asks for one. The PPK2 cannot be that second instrument either, and for a
different reason: Nordic's specification is 200 nA to 1 A of current, a
supplied 0.8 to 5 V in source-meter mode and an accepted 0.8 to 5 V in
ampere-meter mode, with no voltage input anywhere.

The ranking rested on that sentence together with P15's rails, and the P15
half is withdrawn in the next paragraph. What is left of it is one acceptance
item in one lab. It is replaced by a count over all twenty labs, two sections
below.

**P15 is the lab closest to a recorded pass, not P05.** Three of its seven
acceptance items are already satisfied and written into the chapter: the common
ground was shown by a module lighting from a host pin, one module's polarity
was settled by three controls in the order high, low, released, and no jumper
ran between the 5 V rail and any header pin at any point. The semaphore item
asked for a program rather than the three `pinctrl` commands that had stood in
for one, and as of Saturday 17 October 2026 `p15/` is that program, built and
checked.

**Four items remain and none of them wants a voltmeter**, which this page
claimed for two weeks and which re-reading the acceptance test on Saturday 17
October 2026 does not support. Item by item: the rails item asks that both
rails are labelled and that the lab book say what was measured and what could
not be, with the reason named, so it is satisfied by an honest sentence rather
than by a number. The semaphore item wants three modules seen lit at once,
which is a sitting. The "no 5 V on header pins" item is a sentence in the lab
book. And the breadboard item wants the negative-test pull-up gone by the end
of the sitting.

So **P15 reaches passed with one sitting and one resistor.** The resistor is
the part nobody had noticed this lab needs: a 5 V pull-up is a resistor, and
there are no loose resistors on this bench, so the negative test cannot be
built as the chapter describes it and the breadboard item cannot be earned by
a pull-up that was never there. A check that passes because nothing was done
is not a check, which is this volume's own rule.

If the rail numbers are wanted beyond what the item asks, the MCC 118 can take
them: both rails are inside plus or minus 10 V, its ground is already common
with the POW-BB's in this lab's wiring, and that is a separate short step on
the Pi 4 rather than part of the LED sitting on the Pi 3B+.

## What the missing parts actually cost, counted over all 106 items

Derived on Saturday 17 October 2026, after the claim it replaces was
withdrawn. The twenty acceptance tests hold **106 items** between them. The
question a purchase ranking has to answer is how many of those an absent part
is standing in front of, and the answer is smaller than this page has been
implying for two weeks.

**Five items. Out of a hundred and six.**

| Part | Items it unblocks | Where |
| --- | --- | --- |
| A 2x20 stacking or tall header | **2** | P19's first two items. Its third, that no MQTT client is running, needs nothing |
| A multimeter | **1** | P05's seventh, the only item in the volume that asks for a second instrument |
| One resistor, any value | **1** | P15's sixth. The negative test is a 5 V pull-up, and a pull-up is a resistor |
| A soldering iron and one 8-pin 2.54 mm header | **1** | P01's first. A fist producing exactly one message is a two-condition observation, and the SEN0032's friction contacts are reliably present in neither condition |

**So there is no single purchase that unblocks the most acceptance tests, and
the question was the wrong one.** Three of those four parts each carry one lab
to **passed** once a sitting is given to it: the resistor takes P15, the
multimeter takes P05, the stacking header takes P19. They cost very little
between them, so the useful answer is to buy all three rather than to rank
them. The soldering iron does not take P01 anywhere on its own, because two of
P01's other items want a mobile network.

**What is actually in front of the other hundred and one items is bench time,
and after that, mobile data.** Sixteen items across P01, P02, P03 and P13
need a SIM, which is three times what every missing part on the bench costs
put together.

**That question was opened and answered on Saturday 17 October 2026, and the
answer splits the sixteen in two.** There is a SIM here: an Elisa Finnish
prepaid. Finland and Austria are both in the EU, so ordinary LTE data roams,
and **P01's two items are the ones that opens**, because a Cat-4 gateway wants
nothing more exotic than an LTE bearer.

**The other fourteen are blocked by the kind of SIM rather than by its
absence.** A consumer prepaid is not provisioned for NB-IoT or LTE-M; those
want an IoT SIM. NB-IoT roaming is worse again, because it rests on explicit
carrier-to-carrier agreements rather than on the general roaming framework,
while LTE-M roaming is widely established. So P02 at three items, P03 at five
and P13 at six are waiting on **one IoT SIM with LTE-M and NB-IoT roaming into
Austria**, which costs about what a multimeter does.

**That makes an IoT SIM the purchase that unblocks the most acceptance items
in this volume, at fourteen**, against five for every absent part put
together. The question this page asked for two weeks was which tool to buy,
and the answer was never a tool.

**It should be settled at the bench before it is bought**, because the claim
above is about consumer prepaid SIMs in general and not about this one. All
three modem HATs are here and the test is one sitting: put the Elisa SIM in
the SIM7600E-H and confirm an LTE attach, then in the SIM7020E and the
SIM7070G and read what the network grants. A refusal there is a finding
rather than a fault.

**One gap in two acceptance tests, found by answering this.** P02's fifth item
and P03's fifth both say that without a SIM the lab stops at AT bring-up and
that this is still a pass. Neither says anything about the state this bench is
now in, which is a SIM that attaches on LTE and is refused the bearer the lab
is about. That is a third outcome and the chapters name two. Whether it should
be written in as a pass with the reason recorded, the way P15's rails are, is
a decision about the standard and not a tidy-up, so it is left here rather
than made.

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
