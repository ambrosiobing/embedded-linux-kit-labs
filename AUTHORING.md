# Authoring guide for the illustrated kit cookbook

This volume re-publishes the audited 20-lab cookbook with the diagrams it never
had. **`SOURCE.md` is the authority for every fact.** Read it before writing a
line. Do not add hardware it does not list, do not contradict its verdicts, and
do not soften its corrections. Your job is to keep its content and its voice, and
to add: a system architecture diagram, a real schematic with a wiring table, a
bench layout drawing, a UML view, an ASCII data flow, and a short pitfalls list.

Read `sections/p01.tex` and `figures/p01_*.tex` first: they are the reference for
tone, depth and figure style.

Every section must compile alone with

    python build.py --check sections/pNN.tex

and finish with `== RESULT: CLEAN`. Then `python lint.py sections/pNN.tex` must
print `clean`. Then render the pages and look at the figures:

    pdftoppm -r 60 -png build/check_pNN.pdf build/chk_pNN

A label must never sit on top of a symbol or another label. Circuit labels go
beside the branch, not centred on the component. Sequence-diagram notes go
outside the lifelines. Fix and re-render until the figures are clean.

## The twenty labs (numbering is fixed by the source)

| NN | Title | Host and exclusive part | Owns |
|----|-------|--------------------------|------|
| 01 | LTE Cat-4 motion-triggered gateway | Pi 4 + SIM7600E-H + ADXL345 | the only Cat-4 default route, plus shock-event compression |
| 02 | NB-IoT PSM/eDRX CoAP field node | Pi 3 + SIM7020E | NB-IoT and the power-saving duty cycle |
| 03 | Cat-M MQTT node with GNSS stamp | Pi 3B+ + SIM7070G | Cat-M and the GNSS-timestamp-in-payload rule |
| 04 | STWIN.box vibration and ultrasound USB gateway | Pi 4 + STWIN.box + DSI | 6 kHz vibration and the ultrasonic microphones |
| 05 | MCC 118 100 kS/s voltage recorder | Pi 4 + MCC 118 | the only calibrated 10 V analog path |
| 06 | 8x8 ToF occupancy kiosk | Pi 3 + 3.5 inch LCD + Nucleo + 53L8A1 | multi-zone ranging and the SPI heat map |
| 07 | Consumer 9-DoF plus climate (IKS4A1) | Nucleo + IKS4A1, Pi records | consumer MEMS, humidity and the Qvar electrode |
| 08 | Industrial high-g and dual-scale baro (IKS5A1) | Nucleo + IKS5A1, Pi records | simultaneous low-g and high-g, and the 4060 hPa scale |
| 09 | NanoPi NEO Air headless hub and honest eMMC logging | NanoPi NEO Air + ADXL345 | the only non-Raspberry host and the eMMC wear question |
| 10 | PPK2 energy characterisation bench | Pi 4 + PPK2 + ESP32 DUT | microamp figures |
| 11 | Explorer700 field console | Pi 3B+ + Explorer700 | DS3231 as hwclock, the OLED, IR and the overlay lab |
| 12 | Official DSI operator glass and MQTT broker | Pi 4 + DSI touchscreen + keyboard | the panel, the keyboard and the broker |
| 13 | Dual-radio signalling failover | Pi 4 + SIM7600 and Pi 3 + SIM7020 | multi-homing with a source tag |
| 14 | USB-TTL provisioning jig | Pi 3 + USB-TTL + ESP32 and ESP8266 | the Renkforce cable as a factory tool |
| 15 | POW-BB mixed-voltage discipline | POW-BB + LEDs + any host | the electrical safety lab |
| 16 | ESP32 Wi-Fi/BLE sidecar | Pi 4 + ESP32 | the sidecar pattern and LED actuation |
| 17 | ESP8266 AT modem on the NanoPi | NanoPi + ESP8266, onboard Wi-Fi off | the air-gap radio |
| 18 | Nucleo USB-CDC recorder | Pi 4 + bare Nucleo | high-rate USB gadget traffic |
| 19 | SPI LCD tilt meter | Pi 3 + 3.5 inch LCD + ADXL345 | the LCD as an offline field instrument |
| 20 | Four-host fleet health | all four Linux hosts + SSH | orchestration, no new physics |

Cross-reference other labs as `P07` in prose (`\ref{sec:p07}` also works). Repeat
the source's own cross-references, for example that P19 and P06 cannot share a Pi
in one sitting, and that P13 reuses the P01 and P02 attachments.

## Section structure (this order exactly)

```
\lab{NN}{Title}{Host and exclusive part}{What it owns}

\begin{unique} ... \end{unique}                 one short paragraph, from SOURCE.md
\begin{blueprint} ... \end{blueprint}           ONLY for labs where SOURCE.md has one
\subsection*{Intent}                            the source's intent, expanded to 2 short paragraphs
\begin{kit} ... \end{kit}                       the parts line from SOURCE.md
\subsection*{System architecture}               \diagram{pNN_arch}{...} + 1 paragraph
\subsection*{Wiring and schematic}              \diagram{pNN_schematic}{...} + wiring table with
                                                real pin numbers + the source's ASCII wiring block
\subsection*{Bench layout}                      \diagram{pNN_bench}{...} + 1 short paragraph
\subsection*{Software design (UML)}             \diagram{pNN_uml}{...} + 1 paragraph
\subsection*{Data flow (ASCII)}                 \begin{asciiart} ... \end{asciiart}
\subsection*{Steps}                             \begin{steps} ... \end{steps}, the source's steps,
                                                verbatim in substance, with its commands and code
\subsection*{Acceptance test}                   the source's acceptance test, as measurable bullets
\subsection*{Practices}                         the source's practices
\subsection*{Pitfalls}                          bullets: what goes wrong and the symptom it shows
\subsection*{Sources}                           bullets with \url{...}: vendor wikis, datasheets, docs
```

Target length: 250 to 420 lines of LaTeX per lab, which lands at 6 to 9 PDF pages.
The short labs of the source (P15, P16, P17, P19, P20) stay short; do not pad them
to match P01. Keep every command, address and number from `SOURCE.md` exactly.

## LaTeX subset (the HTML converter understands exactly this)

- Headings: `\subsection*{}`, `\subsubsection*{}`, `\paragraph{}`.
- Inline: `\textbf{}`, `\emph{}`, `\texttt{}`, `\url{}`, `\href{}{}`, `\verb|...|`, `\ref{sec:pNN}`, `\newline`, `\ldots`, `\textmu{}`, `\textdegree{}`, `\textohm{}`, simple `$...$` math (`\mu`, `\Omega`, `\pm`, `\times`, `\leq`, `\geq`, `\approx`, `^{}`, `_{}`).
- Lists: `itemize`, `enumerate`, `description` (`\item[Label]`), `steps`.
- Boxes: `unique`, `blueprint`, `kit` (no argument each), `\begin{note}{Title}`, `\begin{rulebox}{Title}`.
- Code: `atcode` (AT transcripts), `shellcode`, `pycode`, `ccode`, `dtscode`, `yamlcode`, `plaincode`, `asciiart`. Pure ASCII inside, no box-drawing characters, no Greek, no arrows. Code lines at most 99 characters, `asciiart` lines at most 112.
- Tables: `\begin{table}[H]\centering\small \begin{tabular}{...} \toprule ... \midrule ... \bottomrule \end{tabular} \caption{...} \end{table}`. Use `p{..mm}` columns for long text, total width at most 165 mm. Escape `_`, `&`, `%`, `#` in prose and tables.
- Figures: only through `\diagram{name}{Caption}` with `figures/name.tex` holding exactly one `tikzpicture` or `circuitikz` environment.
- Do NOT use: `\section`, `\footnote`, `\includegraphics`, `minipage`, `\newcommand`, `\input`, `\cite`, unicode arrows, en or em dashes, `--` or `---` in prose. Use a colon, a comma or "to" instead of a dash.

## Figure conventions (see tikz_preamble.tex)

Styles: `hw` (yellow, hardware), `kern` (blue, kernel or driver), `user` (green, user space), `ext` (grey, PC, cloud, network), `fw` (pink, MCU firmware), `wide`, `narrow`, `layer` (dashed fit box) with `layerlabel`, arrows `flow`, `bus`, `irq`, edge labels `lbl`. UML: `actor`, `lifeline`, `msg`, `reply`, `activation`, `state`, `initial`, `final`, `trans`, `umlnote`, `umlclass`. Bench art: `board`, `hat`, `breadboard`, `pinrow`, `cable`, `usbcable`, `ledsym`. circuitikz is loaded `[european]`: `to[R, l=...]`, `to[led]`, `node[ground]`, `to[short]`, `to[C]`, `to[D]`.

Colour the MCU firmware boxes `fw` and the Linux user-space boxes `user`, so the
book's central rule (Linux owns the system, the MCU is a peripheral) is visible at
a glance in every architecture figure.

Every figure at most 16 cm wide and 12 cm high. Escape `_` and `&` in node text.
Raspberry Pi 40-pin numbering: pin 1 3V3, 2 5V, 3 SDA1/GPIO2, 5 SCL1/GPIO3, 6 GND,
8 TXD0/GPIO14, 10 RXD0/GPIO15, 11 GPIO17, 12 GPIO18, 13 GPIO27, 15 GPIO22,
16 GPIO23, 18 GPIO24, 19 MOSI/GPIO10, 21 MISO/GPIO9, 22 GPIO25, 23 SCLK/GPIO11,
24 CE0/GPIO8, 26 CE1/GPIO7, 29 GPIO5, 31 GPIO6, 32 GPIO12, 33 GPIO13, 35 GPIO19,
36 GPIO16, 37 GPIO26, 38 GPIO20, 40 GPIO21. GND on 6, 9, 14, 20, 25, 30, 34, 39.
NanoPi NEO Air 24-pin: 1 SYS_3V3, 2 VDD_5V, 3 I2C0_SDA, 5 I2C0_SCL, 6 GND,
8 UART1_TX (PG6), 10 UART1_RX (PG7), plus a separate 4-pin debug UART0 header.

## Writing rules

- Faithful first. Every command, AT string, I2C address, threshold and acceptance
  number comes from `SOURCE.md`. If you need a detail the source does not give,
  either leave it out or write "confirm against the vendor wiki" in the text.
- Keep the source's blunt register. It says "a continuous ping is a fail" and
  "do not"; keep that. Do not add marketing tone and do not hedge its verdicts.
- Professional wording, no violent idioms (no kill, attack, fight, blame, hurt,
  destroy; write "terminate the process", "the cause is", "leaves the cable unusable").
- No em or en dashes and no `--` in prose, anywhere.
- Nothing anywhere in the text about how it was produced, or about the tools
  that helped produce it. This rule deliberately does not name them: a rule
  that spells the words it forbids puts those words into the repository it is
  meant to keep them out of, which is the opposite of what it is for.
- Each lab stays unique: when a technique belongs to another lab, point at it
  rather than repeating it, exactly as the source does.
- Safety notes where the source gives them: 3.3 V logic only, never 5 V into a
  GPIO or an ESP pin, never exceed 10.1 V on an MCC 118 input, never source a
  Cat-4 power amplifier from the PPK2, no antenna means no attach.
