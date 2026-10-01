"""The lab's own acceptance claim, rehearsed against synthetic stimuli.

The chapter states the demonstration before it states the wiring: touch the
case and the vibration energy rises; speak near the microphone and the
microphone level rises while the vibration band does not move. Two physical
quantities, two independent paths through one cable, and a host that can tell
them apart.

These tests assert exactly that, on signals this project generates. Passing
them does not mean the bench will pass, because the stimuli are clean in a way
a real knuckle and a real voice are not. What passing them does mean is that
the pipeline is capable of expressing the distinction, so a failure on the
bench will be a fact about the mounting or the probe rather than about the
software. That is the whole reason for building this half first.

The band is named in hertz here rather than left to the default, for a reason
that cost the first run of this suite and is recorded in BandChoice below.
"""

import unittest

import numpy as np

from benchkit.analysis import Thresholds, band_bins, band_energy, rms, spectrum
from p04.pipeline import run
from p04.synth import stream

RATE = 26667.0          # IIS3DWB output data rate, to be confirmed on the firmware
BLOCK = 2048
SEED = 7

# The sensor is flat to about 6 kHz, so the band that can carry a case
# resonance is below that, not above it. See BandChoice for what happens when
# this is left to the default.
BAND_LOW, BAND_HIGH = 2000.0, 6000.0


def energies(kind, seed=SEED, blocks=8, low=BAND_LOW, high=BAND_HIGH):
    """Mean vibration band energy and mean microphone level for one stimulus."""
    vib, mic = [], []
    for b in stream(kind, blocks, BLOCK, RATE, seed):
        vib.append(band_energy(b.vib, RATE, low, high))
        mic.append(rms(b.mic))
    return float(np.mean(vib)), float(np.mean(mic))


class Orthogonality(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.idle_vib, cls.idle_mic = energies("idle")
        cls.touch_vib, cls.touch_mic = energies("touch")
        cls.speech_vib, cls.speech_mic = energies("speech")

    def test_touch_raises_the_vibration_band(self):
        self.assertGreater(self.touch_vib, self.idle_vib * 1.5,
                           "a case touch must lift the vibration band well clear of idle")

    def test_touch_leaves_the_microphone_alone(self):
        # The tolerance is deliberately loose. Both channels carry independent
        # noise, so the microphone level wanders a little between runs even
        # when nothing is driving it, and a tight bound here would be a test of
        # the random seed rather than of the separation.
        self.assertLess(abs(self.touch_mic - self.idle_mic), self.idle_mic * 0.25,
                        "a case touch must not move the microphone channel")

    def test_speech_raises_the_microphone(self):
        self.assertGreater(self.speech_mic, self.idle_mic * 1.5,
                           "a voice must lift the microphone level well clear of idle")

    def test_speech_leaves_the_vibration_band_alone(self):
        self.assertLess(abs(self.speech_vib - self.idle_vib), self.idle_vib * 0.25,
                        "a voice must not move the vibration band")

    def test_speech_does_not_leak_into_the_vibration_channel_at_all(self):
        """Cross-talk, checked where the voice actually lives.

        The test above is blind to this on its own: it watches 2 to 6 kHz and
        the voice is a 180 Hz fundamental with two harmonics, so a voice
        leaking wholesale into the vibration channel would not move it. That
        was found by introducing exactly that defect and watching the suite
        stay green, which is the only way to learn it. So this test watches the
        band the voice occupies, on the channel it must not reach.
        """
        voice_low, voice_high = 100.0, 700.0
        idle_v, _ = energies("idle", low=voice_low, high=voice_high)
        speech_v, _ = energies("speech", low=voice_low, high=voice_high)
        self.assertLess(speech_v, idle_v * 1.2,
                        "a voice must not appear in the vibration channel, "
                        "in the band where a voice would show")

    def test_touch_does_not_leak_into_the_microphone_spectrum(self):
        """The mirror of the test above, on the other channel.

        Mean square level alone can hide a narrow addition under broadband
        noise, so this watches the microphone in the band the ring occupies.
        """
        idle_m = float(np.mean([band_energy(b.mic, RATE, BAND_LOW, BAND_HIGH)
                                for b in stream("idle", 8, BLOCK, RATE, SEED)]))
        touch_m = float(np.mean([band_energy(b.mic, RATE, BAND_LOW, BAND_HIGH)
                                 for b in stream("touch", 8, BLOCK, RATE, SEED)]))
        self.assertLess(touch_m, idle_m * 1.2,
                        "the case ring must not appear in the microphone channel")

    def test_the_two_stimuli_are_distinguishable_by_the_classifier(self):
        """The decision, not just the numbers behind it."""
        baseline, _ = energies("idle", seed=SEED + 1)
        t = Thresholds(baseline=baseline, warn=1.5, fault=3.0)

        def worst(kind):
            return run(stream(kind, 6, BLOCK, RATE, SEED + 2), t,
                       RATE, BAND_LOW, BAND_HIGH).worst()

        self.assertEqual(worst("idle"), "NORMAL")
        self.assertIn(worst("touch"), ("WARNING", "FAULT"))
        self.assertEqual(worst("speech"), "NORMAL",
                         "a voice must not be classified as a machine fault")


class BandChoice(unittest.TestCase):
    """Why the band is named in hertz, recorded as a test rather than a comment.

    The chapter's outline takes the high-frequency band as the upper half of
    the spectrum, `spec[len(spec)//2:]`. Read literally with this sensor at
    this output data rate, that band runs from about 6.7 kHz to about 13.3 kHz,
    which is entirely above the range the wideband accelerometer is flat over.
    A case resonance in the low kilohertz therefore lands outside the measured
    band and the classifier reports NORMAL while the structure is ringing.

    This is not a defect in the arithmetic. It is a reminder that the band is a
    property of the machine under test and of the sensor, and that the default
    is only safe while the output data rate is still unknown.
    """

    def test_the_default_band_sits_above_the_sensors_flat_range(self):
        bins = len(spectrum(np.zeros(BLOCK)))
        lo, _ = band_bins(bins)
        lo_hz = lo / (bins - 1) * (RATE / 2)
        self.assertGreater(lo_hz, 6000.0,
                           "at this rate the default band starts above the 6 kHz "
                           "the IIS3DWB is specified flat to")

    def test_a_tap_at_the_block_edge_is_attenuated_by_the_window(self):
        """A second property of the pipeline the chapter does not mention.

        A Hann window is near zero at both ends of the block. A tap that lands
        at the start therefore contributes about a third of its weight, and the
        same tap placed mid-block contributes far more. One tap can read as
        WARNING or as NORMAL depending only on where the block boundary fell,
        which argues for taking the worst of several blocks rather than the
        mean of them, and for overlapping blocks if a single tap must be caught.
        """
        rng_seed = 21
        idle_e, _ = energies("idle", seed=rng_seed)

        def tap_energy(offset_s):
            vals = []
            rng = np.random.default_rng(rng_seed)
            from p04.synth import _clip16, case_touch
            for _ in range(8):
                vib, _m = case_touch(BLOCK, rng, RATE, decay_s=0.015, offset_s=offset_s)
                vals.append(band_energy(_clip16(vib), RATE, BAND_LOW, BAND_HIGH))
            return float(np.mean(vals))

        at_edge = tap_energy(0.0) / idle_e
        mid_block = tap_energy(0.030) / idle_e
        self.assertGreater(mid_block, at_edge * 1.3,
                           "the same tap must read higher away from the block edge")

    def test_the_default_band_misses_a_resonance_the_named_band_catches(self):
        idle_default, _ = energies("idle", low=None, high=None)
        touch_default, _ = energies("touch", low=None, high=None)
        idle_named, _ = energies("idle")
        touch_named, _ = energies("touch")

        self.assertLess(touch_default / idle_default, 1.1,
                        "the default band cannot see a 4.2 kHz ring, by construction")
        self.assertGreater(touch_named / idle_named, 1.5,
                           "the named band sees the same ring clearly")


if __name__ == "__main__":
    unittest.main()
