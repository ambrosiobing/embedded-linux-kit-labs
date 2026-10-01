"""The arithmetic of lab P04, as pure functions.

Nothing here opens a device, reads a clock or keeps state between calls. That
is deliberate and it is what one acceptance criterion of the lab actually
tests: the same capture file, replayed offline, must produce the same
classification. A pipeline that disagrees with itself on a second pass has
state it should not have, and the cheapest way to not have it is to put every
decision in a function whose output depends only on its arguments.

The band is the upper half of the spectrum by default, which is what the
chapter's outline uses. It can be given in hertz instead once a sample rate is
known, because "the high-frequency band" is a property of the machine under
test rather than of the arithmetic.
"""

from __future__ import annotations

from dataclasses import dataclass
from functools import lru_cache

import numpy as np

NORMAL = "NORMAL"
WARNING = "WARNING"
FAULT = "FAULT"


@lru_cache(maxsize=8)
def hann(n: int) -> np.ndarray:
    """A Hann window of length n, computed once per length.

    Cached because the window never changes and a block arrives many times a
    second. numpy.hanning is a cheap call, but this is the one place a cache
    is free of risk: the result depends on nothing but n.
    """
    return np.hanning(n)


def spectrum(block: np.ndarray) -> np.ndarray:
    """Magnitude spectrum of one block, Hann windowed.

    The block is converted to float before windowing. Leaving it as int16
    would overflow on the multiply for loud blocks, which shows up as a
    spectrum that is wrong only when the signal is interesting.
    """
    x = np.asarray(block, dtype=np.float64)
    return np.abs(np.fft.rfft(x * hann(len(x))))


def band_bins(n_bins: int, sample_rate_hz: float | None = None,
              low_hz: float | None = None, high_hz: float | None = None) -> tuple[int, int]:
    """Which spectrum bins the band covers.

    With no sample rate the band is the upper half of the spectrum, which is
    the chapter's outline and is honest when the output data rate has not been
    read off the firmware yet. With a sample rate the band is given in hertz,
    which is what a bearing or a resonance is actually specified in.
    """
    if sample_rate_hz is None or low_hz is None:
        return n_bins // 2, n_bins
    nyquist = sample_rate_hz / 2.0
    hi = high_hz if high_hz is not None else nyquist
    lo_bin = int(round(low_hz / nyquist * (n_bins - 1)))
    hi_bin = int(round(hi / nyquist * (n_bins - 1))) + 1
    lo_bin = max(0, min(lo_bin, n_bins - 1))
    hi_bin = max(lo_bin + 1, min(hi_bin, n_bins))
    return lo_bin, hi_bin


def band_energy(block: np.ndarray, sample_rate_hz: float | None = None,
                low_hz: float | None = None, high_hz: float | None = None) -> float:
    """Summed magnitude over the band. The number the classifier compares."""
    spec = spectrum(block)
    lo, hi = band_bins(len(spec), sample_rate_hz, low_hz, high_hz)
    return float(spec[lo:hi].sum())


def rms(block: np.ndarray) -> float:
    """Root mean square of a block. The acoustic half of the orthogonality test."""
    x = np.asarray(block, dtype=np.float64)
    return float(np.sqrt(np.mean(x * x))) if x.size else 0.0


@dataclass(frozen=True)
class Thresholds:
    """The two ratios and the baseline they belong to.

    They travel together on purpose. The chapter is blunt about this: a
    threshold without its baseline is not a measurement, because the ratio is
    only meaningful against the idle energy it was divided by. There are no
    default values anywhere in this package, and nothing here invents one.
    """

    baseline: float
    warn: float
    fault: float

    def __post_init__(self) -> None:
        if not self.baseline > 0:
            raise ValueError("baseline must be positive; capture one on an idle machine")
        if not self.warn > 1.0:
            raise ValueError("warn must exceed 1.0, since it is a ratio against the baseline")
        if not self.fault >= self.warn:
            raise ValueError("fault must not be below warn")


def classify(energy: float, t: Thresholds) -> tuple[str, float]:
    """Map a band energy to a state and the ratio it was decided on.

    The ratio is returned beside the state because the state alone cannot be
    argued with. A reader who sees WARNING and 1.9 can judge the threshold; a
    reader who sees WARNING can only believe it.
    """
    ratio = energy / t.baseline
    state = NORMAL
    if ratio > t.warn:
        state = WARNING
    if ratio > t.fault:
        state = FAULT
    return state, ratio
