"""Sequence accounting, because a spectrum over a holed stream is a picture of the holes.

The lab requires a two second capture with a monotonic sequence number and no
dropped frames. These tests build streams with holes on purpose, which is
cheaper and more repeatable than unplugging a cable, and they check that the
accounting says so rather than passing quietly.
"""

import unittest

from benchkit.frames import Transport
from p04.pipeline import measure_baseline
from p04.synth import stream

RATE = 26667.0
BLOCK = 256     # short blocks: these tests are about counting, not spectra


def observe(seqs):
    t = Transport()
    for s in seqs:
        t.observe(s)
    return t


class Accounting(unittest.TestCase):

    def test_a_clean_run_is_clean(self):
        t = observe(range(1, 21))
        self.assertTrue(t.clean)
        self.assertEqual((t.gaps, t.missing, t.blocks), (0, 0, 20))
        self.assertIn("gaps 0", t.summary())

    def test_one_hole_is_counted_once_and_sized(self):
        t = observe([1, 2, 3, 7, 8])
        self.assertFalse(t.clean)
        self.assertEqual(t.gaps, 1)
        self.assertEqual(t.missing, 3)            # 4, 5 and 6
        self.assertEqual(t.gap_detail[0], (3, 7))

    def test_two_holes_are_counted_separately(self):
        t = observe([1, 3, 4, 9])
        self.assertEqual(t.gaps, 2)
        self.assertEqual(t.missing, 1 + 4)

    def test_a_repeat_is_out_of_order_not_a_gap(self):
        t = observe([1, 2, 2, 3])
        self.assertEqual(t.gaps, 0)
        self.assertEqual(t.out_of_order, 1)
        self.assertFalse(t.clean)

    def test_a_rewind_is_out_of_order(self):
        t = observe([5, 6, 2, 3])
        self.assertEqual(t.out_of_order, 1)

    def test_an_empty_stream_is_not_clean(self):
        t = observe([])
        self.assertFalse(t.clean, "no blocks is not the same as no gaps")
        self.assertIn("no blocks", t.summary())

    def test_the_gap_record_is_bounded(self):
        t = observe(range(1, 400, 3))         # a hole between every pair
        self.assertGreater(t.gaps, 32)
        self.assertLessEqual(len(t.gap_detail), 32,
                             "a stream losing everything must not also exhaust memory")

    def test_a_dropped_block_reaches_the_accounting_through_the_generator(self):
        blocks = stream("idle", 10, BLOCK, RATE, seed=1, drop=(4, 5))
        t = observe(b.seq for b in blocks)
        self.assertEqual(t.blocks, 8)
        self.assertEqual(t.missing, 2)
        self.assertFalse(t.clean)


class BaselineRefusal(unittest.TestCase):

    def test_a_baseline_needs_blocks(self):
        with self.assertRaises(SystemExit):
            measure_baseline(iter(()))

    def test_a_baseline_is_a_mean_not_a_first_sample(self):
        """Dividing later work by one noisy block puts that noise in every ratio."""
        blocks = list(stream("idle", 12, BLOCK, RATE, seed=3))
        mean, count, t = measure_baseline(iter(blocks))
        self.assertEqual(count, 12)
        self.assertTrue(t.clean)
        from benchkit.analysis import band_energy
        first = band_energy(blocks[0].vib)
        self.assertNotAlmostEqual(mean, first, places=6,
                                  msg="the baseline must average the run, not take its head")


if __name__ == "__main__":
    unittest.main()
