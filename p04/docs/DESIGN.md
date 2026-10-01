# Design: the host half of lab P04

Written Thursday 1 October 2026, before any hardware was attached.

## The boundary

The chapter puts the boundary in one sentence and this repository implements
exactly that sentence. The microcontroller moves samples, stamps them with a
timestamp and a sequence number, and packetises them onto the serial endpoint.
It performs no arithmetic on the signal. Everything downstream of the cable
runs here.

That split is not a preference. The processor in the probe has better access to
the sensors and worse access to everything else: no file system worth the name,
no display, no package manager, and no way to change the analysis without a
reflash. The moment a transform moves into it, the parts of the lab you most
want to change become the parts that are hardest to change.

## Ownership

One owner per thing, which is the table that prevents the commonest class of
defect in this family of projects.

| Thing | Owner | Not owned by |
| --- | --- | --- |
| Sample acquisition and timestamping | the probe firmware | anything here |
| The wire format | the probe firmware | this repository, which must not infer it |
| Sequence accounting | `p04.frames.Transport` | the analysis, which must not hide a hole |
| The window, the transform, the band | `p04.analysis` | the pipeline, which only sequences them |
| The baseline and the two thresholds | the lab book file | any default in any module |
| The band in hertz | the lab book file | the chapter's upper-half fallback, once a rate is known |
| Deciding NORMAL, WARNING or FAULT | `p04.analysis.classify` | the reader, the source, the command line |
| Where blocks come from | `p04.sources` | the analysis, which never learns |

## What is deferred, and why

**The binary frame layout.** `read_binary` raises rather than guessing. A
decoder inferred by inspection works on the build it was inferred from and
silently misreads the next one, and a misread spectrum looks like a
measurement. The refusal names ST's host decoder and the function that brings
its output back into this pipeline, because a refusal without a way forward
gets switched off.

**The text protocol's exact shape.** `read_text_lines` accepts one plausible
line format and skips anything else. That format is this project's reading of a
text dashboard, not a specification of one, and it is the single place to change
when the flashed firmware is known. It is deliberately one small function.

**Every number that would go in a lab book.** The baseline, the two
thresholds, the band edges and the mounting all come from the bench. The
package has no defaults for any of them and `Thresholds` refuses to be
constructed without a positive baseline and a warn ratio above one.

## Two properties found while building, not while reading

**The default band sits above the sensor's flat range.** Taking the upper half
of the spectrum, as the chapter's outline does, gives roughly 6.7 kHz to
13.3 kHz at the accelerometer's output data rate, and the part is specified flat
to about 6 kHz. A resonance in the low kilohertz lands outside the band and
reads as NORMAL. Recorded as `BandChoice` in the orthogonality tests.

**A transient at a block edge loses most of its weight.** A Hann window is near
zero at both ends, so a decaying tap at the start of a block contributes about a
third of what the same tap contributes mid-block, measured. A single tap can
therefore read WARNING or NORMAL depending only on where the block boundary
fell. Recorded as a test. The consequences, which this repository has not yet
implemented: report the worst of several blocks rather than the mean, and
overlap blocks if a single tap must be caught.

## Two tests that did not fire, and now do

Both guards were introduced, then the defect they exist to catch was
introduced, and both stayed green. A guard that never fires is
indistinguishable from one that passes.

**Cross-channel leakage.** The orthogonality test watched 2 kHz to 6 kHz on the
vibration channel. The voice is a 180 Hz fundamental with two harmonics, so
piping the whole voice into the vibration channel moved nothing it was
watching. It now also watches 100 Hz to 700 Hz on that channel, and the mirror
case on the microphone.

**State in the acoustic half.** The replay test compared the classification and
the mean ratio, both of which come from the vibration path. Drift introduced
into the level calculation survived it. It now compares every field of every
reading.

Both were then re-broken and both fired, and went quiet when the defect was
removed. Both directions, which is the only proof worth having.

## What this cannot tell you

Nothing here is a measurement of hardware. The stimuli are three signals chosen
to have the property the lab turns on, which is that one moves the vibration
band and not the microphone and another does the opposite. Real taps are
audible and real rooms are noisy, so the bench numbers will be less clean. What
passing proves is that the pipeline can express the distinction, so a failure on
the bench is a fact about the mounting or the probe rather than about this code.
