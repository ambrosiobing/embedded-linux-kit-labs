"""Whether the sample rate a lab book claims is the rate the data arrived at.

The band edges of a spectrum are given in hertz, and turning hertz into bins
needs a sample rate. Taking that rate from a configuration file and never
checking it makes every band edge an assertion: if the firmware is running at a
different output data rate than the lab book says, every edge is wrong by that
ratio and the classifier still returns a confident answer with no symptom.

The bench rule this implements was learned on a polled bus, where the record is
indexed by the poll rather than by the sensor sample and the two clocks are
unrelated. One timing of the loop converts every frequency from cycles per
sample into hertz and retires the assumption the whole analysis rests on, and
skipping it is the error already paid for once.

This lab is on the better side of that problem by design, because the probe
samples on the sensor's own clock and reads out in bursts, so the record really
is indexed by sensor sample. What remains is the weaker but still real question
of whether the configured rate is the claimed one, and the probe already stamps
the data with what is needed to answer it.
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass
class Timing:
    """Observed rate, accumulated from the timestamps the probe supplies."""

    block_len: int
    first_us: int | None = None
    last_us: int | None = None
    blocks: int = 0
    stamped: int = 0

    def observe(self, t_us: int | None) -> None:
        self.blocks += 1
        if t_us is None:
            return
        self.stamped += 1
        if self.first_us is None:
            self.first_us = t_us
        self.last_us = t_us

    @property
    def usable(self) -> bool:
        """Two stamped blocks at different times is the minimum for a rate."""
        return (self.stamped >= 2 and self.first_us is not None
                and self.last_us is not None and self.last_us > self.first_us)

    def observed_rate_hz(self) -> float | None:
        """Samples per second, from the span between the first and last stamp.

        The span covers one block fewer than were read, because both stamps mark
        the first sample of their own block. Using the block count directly
        overstates the rate by one block's worth, which at sixteen blocks is six
        per cent and would sail through any loose tolerance.
        """
        if not self.usable:
            return None
        span_s = (self.last_us - self.first_us) / 1e6
        return (self.stamped - 1) * self.block_len / span_s

    def check(self, claimed_hz: float | None, tolerance: float = 0.02) -> tuple[bool, str]:
        """Compare the claimed rate with the observed one.

        Returns whether the claim stands and a sentence saying why, in both
        directions. A refusal that does not name the two numbers cannot be
        argued with, so the reader either believes it or switches it off.
        """
        if claimed_hz is None:
            return True, "no rate claimed, so the band is the upper half of the spectrum"
        if not self.stamped:
            return True, (f"claimed {claimed_hz:.1f} Hz, unverified: no block carried a "
                          "timestamp, so the frequency axis rests on the lab book alone")
        if not self.usable:
            return True, (f"claimed {claimed_hz:.1f} Hz, unverified: {self.stamped} stamped "
                          "block is not enough to measure a rate")
        observed = self.observed_rate_hz()
        error = abs(observed - claimed_hz) / claimed_hz
        if error <= tolerance:
            return True, (f"claimed {claimed_hz:.1f} Hz, observed {observed:.1f} Hz "
                          f"over {self.stamped} stamped blocks, within {tolerance:.0%}")
        return False, (f"claimed {claimed_hz:.1f} Hz but observed {observed:.1f} Hz "
                       f"over {self.stamped} stamped blocks, which is {error:.1%} out. "
                       "Every band edge in hertz is wrong by that ratio. Fix the rate in "
                       "the lab book, or the output data rate on the probe, before "
                       "believing any frequency in this run.")
