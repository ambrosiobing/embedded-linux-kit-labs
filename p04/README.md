# P04: the host half of the vibration and ultrasound gateway

The Linux side of lab P04, written and tested before the probe is wired. Every
command but `live` runs with no hardware attached, so the pipeline, its tests
and its defects are all dealt with while the probe is still in its box. When it
arrives, the remaining work is one USB cable and a real baseline.

**What it does.** Reads sequence-numbered blocks of two channels, wideband
vibration and a digital microphone, checks the stream for holes, windows and
transforms them, sums a named band, and classifies the result against a
baseline you measured as NORMAL, WARNING or FAULT.

**State: written, never run against hardware.** Thirty-five tests pass on a
laptop with no probe, no serial port and no Raspberry Pi. No number here is a
measurement of anything physical. The synthetic stimuli are signals with the
right shape, not a model of the probe, and a threshold tuned against them is a
threshold tuned against them.

## With no hardware

    python -m p04 synth --kind idle   --blocks 10 --out idle.npz
    python -m p04 synth --kind touch  --blocks 10 --out touch.npz
    python -m p04 synth --kind speech --blocks 10 --out speech.npz

    python -m p04 baseline --source idle.npz --mounting "synthetic, no probe" \
        --warn 1.5 --fault 3.0 --band-low 2000 --band-high 6000 --out book.json

    python -m p04 analyse --source touch.npz  --book book.json
    python -m p04 analyse --source speech.npz --book book.json
    python -m p04 replay  --source touch.npz  --book book.json

The last three reproduce the lab's own demonstration. A case touch reads
WARNING with the microphone unmoved; a voice reads NORMAL with the microphone
level an order of magnitude higher. That separation is the lab.

## Two findings from building it

**The chapter's default band sits above the sensor.** The outline takes the
high-frequency band as the upper half of the spectrum. At the accelerometer's
output data rate that runs from about 6.7 kHz to 13.3 kHz, and the part is
specified flat to about 6 kHz, so a 4.2 kHz case resonance lands outside the
measured band and the classifier reports NORMAL while the structure rings. The
band is named in hertz here, and the upper-half default is only safe while the
output data rate is still unknown.

**A tap at a block edge is attenuated by the window.** A Hann window is near
zero at both ends, so a decaying tap that lands at the start keeps roughly a
third of its weight, measured. One tap can read WARNING or NORMAL depending
only on where the block boundary fell. Take the worst of several blocks rather
than the mean, and overlap blocks if a single tap has to be caught.

Both are tests, not comments, so they cannot quietly stop being true.
[`docs/DESIGN.md`](docs/DESIGN.md) carries the reasoning and the ownership
table.

## When the probe is on the desk

1. Flash it, then confirm the serial device exists. Until the firmware declares
   the interface, the connector carries power and a firmware-update interface
   and nothing else.
2. Install pyserial, the only dependency not needed before this point.
3. Capture an idle run, take a real baseline, and write the mounting down in
   the same file.
4. Set the two thresholds where they separate your idle population from your
   stimulus population, on your bench.
5. Run `live`, then repeat the orthogonality test and record both numbers.

Replace the synthetic lab book before quoting any number. The baseline command
prints a warning when its input was synthetic, for exactly this reason.

## This lab's files

| Path | What it is |
| --- | --- |
| `synth.py` | the three stimuli, deterministic, for use with no hardware |
| `pipeline.py` | one pass: transport, then analysis |
| `__main__.py` | the command line |
| `docs/DESIGN.md` | the boundary, what is deferred, and why |

The parts this lab shares with the rest of the volume are in
[`benchkit/`](../benchkit/).
