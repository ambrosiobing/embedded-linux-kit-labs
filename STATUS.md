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
| P19 | written | the pairing audit | one measurement, before anything else: whether the LCD's carrier PCB overhangs header pins 27 to 40. A ruler along the green board's long edge settles it, and about 85 mm means it does. The lab is blocked until then, and its chapter says why |
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

## What is missing from the evidence column, and should not be next time

Neither bench sitting recorded the OS image. The board revision is there,
`Raspberry Pi 3 Model B Rev 1.2` from `/proc/device-tree/model`, and so is the
userspace, libgpiod v2.2.1 over `gpiochip0` on `pinctrl-bcm2835`, but the image
date and the kernel are not. A state of **passed** requires them, because
"Bookworm" alone will not tell the next reader what was running. The three
commands that capture it are `cat /etc/os-release`, `uname -a` and
`cat /proc/device-tree/model`, and their output belongs in the lab book beside
the numbers.
