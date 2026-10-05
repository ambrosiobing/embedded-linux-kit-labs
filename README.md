# Embedded Linux Top 20: the audited kit cookbook

Twenty bench labs on one parts bin and four Linux hosts, with every pairing
checked against headers, voltages and silicon before it was written down. One
40-pin HAT per host. One instrument per lab. An acceptance test you can fail.

**158 pages, 81 figures, 20 labs.** Each lab stands on its own: what it alone
owns, what the earlier draft of this material got wrong, the kit it takes from
the bin, a system architecture figure, a schematic with a wiring table, a bench
layout, a UML view, a data-flow sketch, numbered steps with real commands, an
acceptance test, the practices that keep it honest, its pitfalls and its
sources.

**Read it.** The whole volume is in [`chapters/`](chapters/) as Markdown with
its figures beside it. Start with
[About this edition](chapters/00-about-this-volume.md), or take a lab from the
table below.

**Or build it.** The PDF and a single self-contained HTML file come from the
same source and stay local:

    python build.py --chapter 5

That writes `lab-05-mcc-118-100-ks-s-voltage-recorder.pdf` and a matching
self-contained `.html` with its four figures inlined.

**Contents**
[Read it](chapters/) ·
[What has been run](STATUS.md) ·
[What this is and is not](#what-this-is-and-is-not) ·
[The rules the labs obey](#the-rules-the-labs-obey) ·
[The twenty labs](#the-twenty-labs) ·
[Building](#building) ·
[Checks](#checks) ·
[Layout](#repository-layout) ·
[Requirements](#requirements) ·
[Licence](#licence)

## What this is and is not

**This is an audited cookbook, not a tested codebase.** It began as two
documents: a kit-only twenty-lab split, and a longer architectural draft that
reused the same parts bin and proposed pairings that cannot be built from it.
This edition keeps the ambition of the second and throws out its impossible
wiring, lab by lab, with the correction printed beside the lab it belongs to
rather than hidden in a changelog. Thirteen of the twenty carry such a
correction.

**What each lab commits to is its acceptance test**, written to be checkable on
hardware and to be failable. "A continuous ping is a fail" is a real line in
one of them. A lab that cannot reach its test because the bench lacks something
says so in place of a number, because a blank is honest and an unmeasured
number is a claim the first careful reader will check.

**The parts constrain the labs.** Every lab was written against a fixed
inventory, so none opens with a shopping list. Where a lab would be easier with
an instrument that is not here, it names the substitute.

**This is the companion to a volume of a different kind.** The twenty projects
in [embedded-linux-projects][projects] are software engineering depth: the Yocto
Project, kernel drivers, real time, over-the-air updates, trusted execution.
These twenty are bench integration discipline, and the two share a parts bin
without sharing a lab.

[projects]: https://github.com/ambrosiobing/embedded-linux-projects

## The rules the labs obey

Six rules decide what a lab may do, and every lab in the table was checked
against them. They are in full in
[About this edition](chapters/00-about-this-volume.md).

1. **No new parts.** Antennas are in scope. A data SIM is assumed for four labs
   only, and without one those labs stop at bring-up, which is still a pass.
2. **HAT exclusivity.** Six boards each own the 40-pin header. Fit one. The
   official display does not consume the header.
3. **The NanoPi is 24-pin.** It cannot accept any 40-pin HAT.
4. **3.3 V logic.** Every host pin is 3.3 V, and the breadboard supply is a
   labelled rail splitter rather than a licence to mix.
5. **One instrument per lab.** Six sensor boards are six different
   instruments, never interchangeable.
6. **Linux owns the system.** A microcontroller speaks over USB, a serial line
   or a bus to a host that owns storage, time, the network and the interface.
   A bare-metal sketch is allowed only where a Linux host still records it.

## The twenty labs

The numbering is the cookbook's own, so the labs appear here in reading order
and keep their P-numbers. The parts are the cellular group, the instruments,
the sensor shields on the microcontroller, the hosts and displays, and the
rails, sidecars and fleet that close it.

**Written is not run.** Every lab below ends in an acceptance test that can be
failed, and [`STATUS.md`](STATUS.md) says which of them have actually been
compiled, wired and passed, with the board revision and the date. At the time
of writing one lab has been run on hardware and one has been exercised on
synthetic data. Read that page before trusting any step here.

| # | Lab | Host and exclusive part | Owns |
|---|---|---|---|
| P01 | [LTE Cat-4 motion-triggered gateway](chapters/01-lte-cat-4-motion-triggered-gateway.md) | Pi 4, SIM7600E-H, ADXL345 | the only Cat-4 default route |
| P02 | [NB-IoT PSM/eDRX CoAP field node](chapters/02-nb-iot-psm-edrx-coap-field-node.md) | Pi 3, SIM7020E | NB-IoT and the duty cycle |
| P03 | [Cat-M MQTT node with GNSS stamp](chapters/03-cat-m-mqtt-node-with-gnss-stamp.md) | Pi 3B+, SIM7070G | Cat-M and the timestamp rule |
| P04 | [STWIN.box vibration and ultrasound USB gateway](chapters/04-stwin-box-vibration-and-ultrasound-usb-gateway.md) | Pi 4, STWIN.box | 6 kHz vibration and the microphones |
| P05 | [MCC 118 100 kS/s voltage recorder](chapters/05-mcc-118-100-ks-s-voltage-recorder.md) | Pi 4, MCC 118 | the calibrated analogue path |
| P06 | [8x8 ToF occupancy kiosk](chapters/06-8x8-tof-occupancy-kiosk.md) | Pi 3, LCD, Nucleo, 53L8A1 | multi-zone ranging |
| P07 | [Consumer 9-DoF plus climate](chapters/07-consumer-9-dof-plus-climate-iks4a1.md) | Nucleo, IKS4A1 | consumer MEMS and humidity |
| P08 | [Industrial high-g and dual-scale baro](chapters/08-industrial-high-g-and-dual-scale-baro-iks5a1.md) | Nucleo, IKS5A1 | simultaneous low-g and high-g |
| P09 | [NanoPi NEO Air headless hub](chapters/09-nanopi-neo-air-headless-hub.md) | NanoPi NEO Air, ADXL345 | the only non-Raspberry host |
| P10 | [PPK2 energy characterisation bench](chapters/10-ppk2-energy-characterisation-bench.md) | Pi 4, PPK2, ESP32 | microamp figures |
| P11 | [Explorer700 field console](chapters/11-explorer700-field-console.md) | Pi 3B+, Explorer700 | the real-time clock and the overlay |
| P12 | [Official DSI operator glass and broker](chapters/12-official-dsi-operator-glass-and-mqtt-broker.md) | Pi 4, display, keyboard | the panel and the broker |
| P13 | [Dual-radio signalling failover](chapters/13-dual-radio-signalling-failover.md) | two Pis, two modems | multi-homing with a source tag |
| P14 | [USB-TTL provisioning jig](chapters/14-usb-ttl-provisioning-jig.md) | Pi 3, serial cable, two ESP boards | the cable as a factory tool |
| P15 | [Mixed-voltage discipline](chapters/15-pow-bb-mixed-voltage-discipline.md) | the breadboard supply, any host | the electrical safety lab |
| P16 | [ESP32 Wi-Fi/BLE sidecar](chapters/16-esp32-wi-fi-ble-sidecar.md) | Pi 4, ESP32 | the sidecar pattern |
| P17 | [ESP8266 AT modem on the NanoPi](chapters/17-esp8266-at-modem-on-the-nanopi.md) | NanoPi, ESP8266 | the air-gap radio |
| P18 | [Nucleo USB-CDC recorder](chapters/18-nucleo-usb-cdc-recorder.md) | Pi 4, bare Nucleo | high-rate gadget traffic |
| P19 | [SPI LCD tilt meter](chapters/19-spi-lcd-tilt-meter.md) | Pi 3, LCD, ADXL345 | the display as a field instrument |
| P20 | [Four-host fleet health](chapters/20-four-host-fleet-health.md) | all four hosts | orchestration, no new physics |

[The appendix](chapters/21-appendix.md) carries the lab order and teardown, the
command reference, both header pinouts, which lab owns which part, and a
glossary.

Work in the order the appendix gives if the boards are bare. The rails lab is
first and is not optional, and the fleet file is last so that it reflects a
known sitting.

## The labs' software

A lab's host side can be written and tested long before its hardware is wired,
and one has been. It lives in this repository beside the chapter it belongs to,
because a reader who has just read chapter 4 should not have to go looking.

| Path | What it is |
| --- | --- |
| [`p04/`](p04/) | the host half of the vibration and ultrasound gateway, in C, with its own README |
| [`p04/src/`](p04/src/) | the capture format, the transform, the classification, the invariants |
| [`p04/tests/`](p04/tests/) | the suite, four groups, no framework and no device |

    make -C p04 check     the suite: no hardware, no serial port, no network
    make -C p04 demo      the lab's own demonstration, on synthetic data

**It is C because the lab is embedded Linux.** The deliverable of a lab is the
program, not a description of one. No allocation on the data path, library code
that returns a status rather than exiting, and samples that stay `int16_t`
until the one place where they become floats.

**Python here builds the book and nothing else.** `build.py`, `mdbuild.py` and
`lint.py` turn the sources into chapters, figures and the reading editions.
They do not touch a bus, a device or a lab, and no lab depends on them.

Most of these labs are the same shape: something arrives over a line in
sequence-numbered pieces and a decision is made from it. P02 and P03 drive a
modem, P06 reads a ranging grid, P18 counts gaps in a sequence, P13 decides on
a timeout. When the second of them is written, the shared half moves out of
`p04/src` and into a library beside it, rather than being guessed at now.

Three rules hold across all of them. No default threshold anywhere, because a
threshold without the baseline it was measured against is not a measurement. No
invented wire format, so where a vendor owns the frame layout the program says
so and stops. And transport before signal, because a spectrum computed over a
stream with holes in it is a picture of the holes.

Nothing here has been run against hardware, and nothing has been compiled on
the authoring laptop, which has no compiler. The workflow is the first compile.
Each lab's README says so in its own words.

## Building

    python build.py --chapter 5      one lab, PDF and self-contained HTML
    python build.py --chapters       all twenty, one file each
    python build.py                  the whole volume, PDF and one HTML file
    python mdbuild.py                the Markdown edition, chapters and figures

Built output is not committed. The PDF and the HTML are reading editions, and
they are rebuilt from this source rather than carried in it.

## Checks

    python lint.py                   house rules over every lab, and the parts
    python tests/test_inventory.py   the parts checker, broken seven ways

The linter refuses dashes, non-ASCII inside a code block, a code line too long
to print, and a lab missing any part of its skeleton or any of its four
figures.

It also reads [`inventory.json`](inventory.json), which is rules 2 to 5 of the
front matter written as data rather than prose: every part with the header it
fits, whether it claims that header exclusively, its logic voltage, and the
labs that use it. From that the linter refuses a second board claiming one
host's header, a 40-pin HAT on the NanoPi's 24-pin header, a voltage mismatch,
two of the six instruments in one lab, and the two halves of the file
disagreeing about which lab uses what. The suite breaks the file seven ways to
prove the checker is awake, and five of those seven are pairings the front
matter already lists as errors found in the 198-page blueprint.

What it cannot decide is anything spatial, and P19 is the standing example.
The panel claims the header once and the sensor claims nothing, so the file
passes, and the lab is still blocked because the panel sits physically over the
pins the sensor needs. A rule engine is not a bench. The workflow in `.github/workflows/` runs it on every push, checks
that every lab has its four figures and its lab line, and checks that the
Markdown edition is in step with the source rather than behind it.

It also refuses a [`STATUS.md`](STATUS.md) that has lost a lab or grown a
state. The four states are written, checked, run and passed, and a row whose
state is anything else fails the run, because the usual way a status page rots
is a new word invented in passing to avoid writing down one of the four.

## Repository layout

| Path | What it is |
|---|---|
| `chapters/` | the Markdown edition, one file per lab, generated |
| `figures/` | one TikZ or circuitikz source per figure, and its SVG |
| `sections/` | the LaTeX source, one file per lab |
| `main.tex` | preamble, the lab macro, the callout boxes, part structure |
| `tikz_preamble.tex` | shared figure styles: blocks, UML, bench art |
| `build.py` | figures to SVG, the PDF, and the single-file HTML |
| `mdbuild.py` | the Markdown edition |
| `lint.py` | house-style check, and the parts check |
| `inventory.json` | the bill of materials: parts, headers, voltages, exclusive claims |
| `tests/test_inventory.py` | the parts checker broken seven ways, no framework |
| `STATUS.md` | which labs have been written, checked, run and passed |
| `SOURCE.md` | the cookbook this edition illustrates, transcribed |
| `AUTHORING.md` | the contract every lab follows |
| `CONTENTS.md` | the lab table, generated, which the table above follows |
| `build/` | scratch output, ignored, safe to delete |

Lab files use a `p` prefix so that a cross-reference or a copied figure can
never silently resolve against a sibling volume's files. The volume's identity
lives in exactly one `DOC` block in `build.py`, which refuses to run if the
folder name stops matching it.

## Requirements

MiKTeX or TeX Live with `pdflatex`, `latex`, `dvisvgm`, `circuitikz`,
`tcolorbox` and `listings`; Python 3.10 or newer. No Python package outside the
standard library is needed.

## Licence

MIT, see [`LICENSE`](LICENSE). Each lab's Sources section records where its
prior art came from, because a reader following a vendor wiki or a licence-gated
driver needs to know what that commits them to before writing code around it.
