"""One pass over a stream of blocks: transport accounting, then the analysis.

The order matters and it is the chapter's. The sequence numbers are checked
first and reported with the result, because a spectrum computed over a stream
with holes in it is a picture of the holes. A run that classifies confidently
over a stream that lost a third of its frames has answered a question nobody
asked.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Iterable

from benchkit.analysis import Thresholds, band_energy, classify, rms
from benchkit.frames import Block, Transport
from benchkit.labbook import LabBook


@dataclass
class Reading:
    """What one block produced."""

    seq: int
    state: str
    ratio: float
    vib_energy: float
    mic_rms: float


@dataclass
class Result:
    """What a whole run produced, signal and transport kept apart."""

    readings: list[Reading] = field(default_factory=list)
    transport: Transport = field(default_factory=Transport)

    @property
    def states(self) -> list[str]:
        return [r.state for r in self.readings]

    def worst(self) -> str:
        order = {"NORMAL": 0, "WARNING": 1, "FAULT": 2}
        return max((r.state for r in self.readings), key=lambda s: order[s], default="NORMAL")

    def mean_ratio(self) -> float:
        return sum(r.ratio for r in self.readings) / len(self.readings) if self.readings else 0.0

    def mean_mic_rms(self) -> float:
        return sum(r.mic_rms for r in self.readings) / len(self.readings) if self.readings else 0.0


def run(blocks: Iterable[Block], thresholds: Thresholds,
        sample_rate_hz: float | None = None, band_low_hz: float | None = None,
        band_high_hz: float | None = None) -> Result:
    """Classify every block. Pure in everything but the iterator it consumes."""
    result = Result()
    for b in blocks:
        result.transport.observe(b.seq)
        energy = band_energy(b.vib, sample_rate_hz, band_low_hz, band_high_hz)
        state, ratio = classify(energy, thresholds)
        result.readings.append(Reading(b.seq, state, ratio, energy, rms(b.mic)))
    return result


def run_with(blocks: Iterable[Block], book: LabBook) -> Result:
    """The same, taking the band and the thresholds from a lab book entry."""
    return run(blocks, book.thresholds(), book.sample_rate_hz,
               book.band_low_hz, book.band_high_hz)


def measure_baseline(blocks: Iterable[Block], sample_rate_hz: float | None = None,
                     band_low_hz: float | None = None,
                     band_high_hz: float | None = None) -> tuple[float, int, Transport]:
    """Mean band energy over an idle run, with the transport that produced it.

    Returns the mean rather than the first block's energy. One block of an idle
    machine is a sample of the noise, and dividing every later measurement by
    one sample of noise puts that noise in every ratio the lab ever reports.
    """
    transport = Transport()
    total, count = 0.0, 0
    for b in blocks:
        transport.observe(b.seq)
        total += band_energy(b.vib, sample_rate_hz, band_low_hz, band_high_hz)
        count += 1
    if not count:
        raise SystemExit("no blocks: cannot measure a baseline from an empty stream")
    return total / count, count, transport
