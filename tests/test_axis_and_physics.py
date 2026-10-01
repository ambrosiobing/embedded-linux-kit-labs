"""The two questions a spectrum cannot answer about itself.

A band edge in hertz needs a sample rate, and a count needs a scale. Both are
numbers that arrive from paperwork, and a wrong one produces a confident answer
with no symptom: the classifier still runs, the plot still looks like a plot,
and every frequency or every level is wrong by a constant ratio that nothing in
the output reveals.

The bench rule behind the first is that the record is indexed by the clock
nobody timed, and that one timing of the capture retires the assumption the
whole analysis rests on. The rule behind the second is to prefer a validity
invariant to a plausibility check, because for an accelerometer at rest physics
fixes the answer and a per-axis plot does not.
"""

import unittest

from benchkit.invariants import check_at_rest, dc_level_g
from benchkit.timing import Timing
from p04.synth import COUNTS_PER_G, G_PER_COUNT, stream

RATE = 26667.0
BLOCK = 512


def timing_over(**kwargs):
    t = None
    for b in stream(**kwargs):
        if t is None:
            t = Timing(block_len=len(b))
        t.observe(b.t_us)
    return t


class RateIsMeasuredNotAsserted(unittest.TestCase):

    def test_an_honest_capture_confirms_its_claimed_rate(self):
        t = timing_over(kind="idle", blocks=10, block_len=BLOCK, sample_rate_hz=RATE, seed=1)
        ok, why = t.check(RATE)
        self.assertTrue(ok, why)
        self.assertAlmostEqual(t.observed_rate_hz(), RATE, delta=RATE * 0.001)

    def test_a_probe_running_off_the_claimed_rate_is_caught(self):
        """The fault the check exists for, reproduced rather than described."""
        t = timing_over(kind="idle", blocks=10, block_len=BLOCK, sample_rate_hz=RATE,
                        seed=1, true_rate_hz=30000.0)
        ok, why = t.check(RATE)
        self.assertFalse(ok)
        self.assertIn("26667", why)
        self.assertIn("30000", why)
        self.assertIn("band edge", why,
                      "the refusal must say what the error does, not only that it exists")

    def test_a_small_drift_inside_the_tolerance_is_accepted(self):
        t = timing_over(kind="idle", blocks=12, block_len=BLOCK, sample_rate_hz=RATE,
                        seed=1, true_rate_hz=RATE * 1.005)
        self.assertTrue(t.check(RATE, tolerance=0.02)[0])

    def test_the_same_drift_is_refused_under_a_tighter_tolerance(self):
        t = timing_over(kind="idle", blocks=12, block_len=BLOCK, sample_rate_hz=RATE,
                        seed=1, true_rate_hz=RATE * 1.005)
        self.assertFalse(t.check(RATE, tolerance=0.001)[0])

    def test_an_unstamped_capture_says_so_rather_than_passing_quietly(self):
        t = timing_over(kind="idle", blocks=6, block_len=BLOCK, sample_rate_hz=RATE,
                        seed=1, stamp=False)
        ok, why = t.check(RATE)
        self.assertTrue(ok, "an absent clock is not a wrong clock")
        self.assertIn("unverified", why)
        self.assertIn("lab book alone", why)

    def test_one_stamped_block_cannot_give_a_rate(self):
        t = timing_over(kind="idle", blocks=1, block_len=BLOCK, sample_rate_hz=RATE, seed=1)
        self.assertIsNone(t.observed_rate_hz())
        self.assertIn("unverified", t.check(RATE)[1])

    def test_the_span_covers_one_block_fewer_than_were_read(self):
        """Off by one here overstates the rate by a block, which a loose
        tolerance would wave through."""
        t = timing_over(kind="idle", blocks=16, block_len=BLOCK, sample_rate_hz=RATE, seed=1)
        naive = t.stamped * BLOCK / ((t.last_us - t.first_us) / 1e6)
        self.assertGreater(naive, RATE * 1.05, "the naive form really is wrong by a block")
        self.assertAlmostEqual(t.observed_rate_hz(), RATE, delta=RATE * 0.001)

    def test_no_claimed_rate_means_no_complaint(self):
        t = timing_over(kind="idle", blocks=4, block_len=BLOCK, sample_rate_hz=RATE, seed=1)
        ok, why = t.check(None)
        self.assertTrue(ok)
        self.assertIn("upper half", why)


class PhysicsChecksTheScale(unittest.TestCase):

    def vib(self, **kwargs):
        return [b.vib for b in stream(kind="idle", blocks=8, block_len=BLOCK,
                                      sample_rate_hz=RATE, seed=3, **kwargs)]

    def test_a_probe_at_rest_sits_within_one_g(self):
        check = check_at_rest(self.vib(), G_PER_COUNT)
        self.assertTrue(check.ok, check.reason)
        self.assertLess(abs(check.level_g), 1.05)
        self.assertIn("one-axis bound", check.reason,
                      "the check must say which form of the invariant it applied")

    def test_a_full_scale_wrong_by_a_factor_of_four_is_caught(self):
        """The fault the invariant exists for: counts read against the wrong scale."""
        check = check_at_rest(self.vib(), G_PER_COUNT * 4)
        self.assertFalse(check.ok)
        self.assertIn("full-scale", check.reason)
        self.assertIn("byte order", check.reason)

    def test_a_swapped_byte_order_is_NOT_caught_and_that_is_recorded(self):
        """The reach of this check, measured rather than assumed.

        Writing this test expecting a pass is what found the limit. A byte swap
        turns an honest resting level of about +0.98 g into about -0.42 g on
        this generator, and both sit inside the one-g bound, so the check is
        silent on a stream it ought to refuse. A bound cannot do an equality's
        work: only the three-axis magnitude pins byte order, because a swap that
        leaves one axis plausible will not leave the sum of three squares at one
        g.

        The test asserts the limitation so that it cannot quietly stop being
        true. If the stream ever carries three axes and the stronger form goes
        in, this test fails and says why, which is the point of writing it this
        way round rather than deleting it.
        """
        swapped = [v.astype("<i2").byteswap() for v in self.vib()]
        check = check_at_rest(swapped, G_PER_COUNT)
        self.assertTrue(check.ok,
                        "while the stream carries one axis this check cannot see a "
                        "byte swap; if it now can, the stronger invariant has arrived "
                        "and this test should be replaced by it")
        self.assertLess(abs(check.level_g), 1.05,
                        "the swapped level is wrong and still inside the bound, which "
                        "is exactly why a bound is not enough")
        self.assertNotAlmostEqual(check.level_g, 0.98, places=1,
                                  msg="the swap really did change the level")

    def test_without_a_scale_the_invariant_says_it_cannot_run(self):
        check = check_at_rest(self.vib(), None)
        self.assertTrue(check.ok, "an absent scale is not a failed check")
        self.assertIn("cannot be applied", check.reason)
        self.assertIn("counts per g", check.reason)

    def test_an_empty_capture_fails_rather_than_passing(self):
        self.assertFalse(check_at_rest([], G_PER_COUNT).ok)

    def test_the_level_is_reported_in_g_not_in_counts(self):
        level = dc_level_g(self.vib()[0], G_PER_COUNT)
        self.assertLess(abs(level), 1.05)
        self.assertGreater(abs(level) * COUNTS_PER_G, 100,
                           "the same level in counts is a large number, which is the "
                           "unit the invariant would be meaningless in")


if __name__ == "__main__":
    unittest.main()
