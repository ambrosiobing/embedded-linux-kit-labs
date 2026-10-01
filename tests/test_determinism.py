"""Replay determinism, and the refusals that keep a threshold honest.

Two acceptance criteria of the lab live here. The first: the same capture file,
replayed through the analysis offline, produces the same classification, and if
it does not the pipeline has state it should not have. The second is not phrased
as a test in the chapter but is the reason it gives no threshold values, which is
that a threshold without its baseline is not a measurement. A package that
shipped a default would quietly undo that, so these tests check that it refuses.
"""

import json
import tempfile
import unittest
from pathlib import Path

from benchkit.analysis import Thresholds, band_energy, classify, hann, spectrum
from benchkit.labbook import LabBook
from p04.pipeline import run, run_with
from benchkit.sources import FrameFormatUnknown, read_binary, read_capture, write_capture
from p04.synth import stream

RATE = 26667.0
BLOCK = 512


def capture(tmp, kind, seed=11, blocks=6, drop=()):
    path = Path(tmp) / f"{kind}.npz"
    write_capture(path, stream(kind, blocks, BLOCK, RATE, seed, drop=drop),
                  {"kind": kind, "block_len": BLOCK, "sample_rate_hz": RATE})
    return path


class Determinism(unittest.TestCase):

    def test_two_passes_over_one_file_agree_exactly(self):
        """Every field of every reading, not only the state and the ratio.

        Comparing the states and the mean ratio alone leaves the microphone
        column unchecked, so state introduced into the acoustic half would
        survive this test. That was found by adding exactly such state and
        watching the suite stay green. The comparison is now the whole
        reading, which is the thing that has to be reproducible.
        """
        with tempfile.TemporaryDirectory() as tmp:
            path = capture(tmp, "touch")
            t = Thresholds(baseline=1000.0, warn=1.5, fault=3.0)
            first, _ = read_capture(path)
            second, _ = read_capture(path)
            r1, r2 = run(first, t), run(second, t)
            self.assertEqual(r1.readings, r2.readings,
                             "exact equality, not approximate: there is no clock in this path")
            self.assertEqual(r1.mean_mic_rms(), r2.mean_mic_rms())

    def test_the_analysis_is_a_pure_function_of_its_block(self):
        blocks = list(stream("touch", 3, BLOCK, RATE, seed=5))
        once = [band_energy(b.vib) for b in blocks]
        twice = [band_energy(b.vib) for b in reversed(blocks)][::-1]
        self.assertEqual(once, twice, "order of evaluation must not change a result")

    def test_the_window_cache_returns_the_same_values(self):
        a, b = hann(256), hann(256)
        self.assertIs(a, b, "the cache should hand back the same array")
        self.assertEqual(spectrum([0] * 8).tolist(), spectrum([0] * 8).tolist())


class ThresholdsRefuse(unittest.TestCase):

    def test_a_baseline_of_zero_is_refused(self):
        with self.assertRaises(ValueError):
            Thresholds(baseline=0.0, warn=2.0, fault=3.0)

    def test_a_warn_at_or_below_one_is_refused(self):
        """A ratio of 1.0 is the baseline itself, so everything would be a warning."""
        with self.assertRaises(ValueError):
            Thresholds(baseline=10.0, warn=1.0, fault=3.0)

    def test_fault_below_warn_is_refused(self):
        with self.assertRaises(ValueError):
            Thresholds(baseline=10.0, warn=3.0, fault=2.0)

    def test_classify_reports_the_ratio_beside_the_state(self):
        state, ratio = classify(25.0, Thresholds(baseline=10.0, warn=1.5, fault=3.0))
        self.assertEqual(state, "WARNING")
        self.assertAlmostEqual(ratio, 2.5)

    def test_a_missing_lab_book_says_how_to_make_one(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(SystemExit) as caught:
                LabBook.load(Path(tmp) / "absent.json")
            message = str(caught.exception)
            self.assertIn("baseline", message)
            self.assertIn("p04 baseline", message,
                          "a refusal should name the command that fixes it")

    def test_an_unknown_field_in_a_lab_book_is_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "book.json"
            p.write_text(json.dumps({"baseline": 1.0, "warn": 2.0, "fault": 3.0,
                                     "mounting": "m", "block_len": 8, "oops": 1}),
                         encoding="utf-8")
            with self.assertRaises(SystemExit):
                LabBook.load(p)

    def test_a_lab_book_round_trips(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "book.json"
            book = LabBook(baseline=123.4, warn=1.5, fault=3.0,
                           mounting="magnet on the pump housing", block_len=BLOCK,
                           sample_rate_hz=RATE)
            book.save(p)
            back = LabBook.load(p)
            self.assertEqual(back.baseline, 123.4)
            self.assertEqual(back.mounting, "magnet on the pump housing")
            self.assertIn("magnet", back.describe())

    def test_a_lab_book_drives_the_run(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = capture(tmp, "idle")
            book = LabBook(baseline=1.0, warn=1.5, fault=3.0, mounting="bench",
                           block_len=BLOCK, sample_rate_hz=RATE)
            blocks, _ = read_capture(path)
            result = run_with(blocks, book)
            self.assertEqual(len(result.readings), 6)


class Sources(unittest.TestCase):

    def test_a_capture_round_trips_through_the_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = capture(tmp, "speech")
            blocks, meta = read_capture(path)
            blocks = list(blocks)
            self.assertEqual(len(blocks), 6)
            self.assertEqual(meta["kind"], "speech")
            self.assertEqual(len(blocks[0]), BLOCK)

    def test_an_empty_capture_is_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(ValueError):
                write_capture(Path(tmp) / "empty.npz", [])

    def test_the_binary_path_refuses_and_says_why(self):
        """A blank refusal gets switched off. This one names the tool to use."""
        with self.assertRaises(FrameFormatUnknown) as caught:
            next(read_binary("capture.bin"))
        message = str(caught.exception)
        self.assertIn("HSDatalog", message)
        self.assertIn("write_capture", message,
                      "the refusal should name the way back into this pipeline")

    def test_text_lines_parse_and_malformed_lines_are_skipped(self):
        from benchkit.sources import read_text_lines
        good = "1," + ",".join(["10"] * 4) + ",|," + ",".join(["20"] * 4)
        lines = ["# a comment", "", "rubbish", good, "2,1,2,|,3"]
        blocks = list(read_text_lines(lines, block_len=4))
        self.assertEqual(len(blocks), 1, "the short line and the rubbish are skipped")
        self.assertEqual(blocks[0].seq, 1)
        self.assertEqual(list(blocks[0].mic), [20, 20, 20, 20])


if __name__ == "__main__":
    unittest.main()
