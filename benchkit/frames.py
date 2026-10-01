"""Blocks, and the sequence accounting that decides whether a spectrum is real.

One acceptance criterion of the lab is that a two second capture has a
monotonic sequence number and no dropped frames, and the reason is given in the
chapter in one sentence: a spectrum computed over a stream with holes in it is
a picture of the holes. So the gap count is not a diagnostic that lives beside
the analysis, it is a precondition of it, and it is reported with every run.

A block carries two channels because the lab's demonstration needs both. The
vibration channel is the wideband accelerometer and the microphone channel is
the digital microphone. Touching the case must move the first and not the
second; speaking must move the second and not the first.
"""

from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np


@dataclass(frozen=True)
class Block:
    """One sequence-numbered pair of channels."""

    seq: int
    vib: np.ndarray
    mic: np.ndarray
    # Microseconds on the probe's own monotonic clock, marking the first sample
    # of this block. Optional because a capture written before the firmware was
    # known carries none, and because the text protocol may not expose one until
    # the flashed firmware is read. Where it is present it is what turns the
    # claimed sample rate from an assertion into a measurement.
    t_us: int | None = None

    def __post_init__(self) -> None:
        if len(self.vib) != len(self.mic):
            raise ValueError(
                f"channels differ in length: vib {len(self.vib)}, mic {len(self.mic)}")

    def __len__(self) -> int:
        return len(self.vib)


@dataclass
class Transport:
    """What the stream did, as distinct from what the signal did.

    Kept separate from the analysis because they fail for different reasons and
    a reader needs to tell them apart. A quiet machine and a dropped cable both
    produce an unexciting spectrum.
    """

    first_seq: int | None = None
    last_seq: int | None = None
    blocks: int = 0
    gaps: int = 0
    missing: int = 0
    out_of_order: int = 0
    gap_detail: list[tuple[int, int]] = field(default_factory=list)

    def observe(self, seq: int) -> None:
        self.blocks += 1
        if self.first_seq is None:
            self.first_seq = seq
            self.last_seq = seq
            return
        step = seq - self.last_seq
        if step == 1:
            self.last_seq = seq
            return
        if step <= 0:
            self.out_of_order += 1
        else:
            self.gaps += 1
            self.missing += step - 1
            # Keep a bounded record. A stream that is losing everything should
            # not also exhaust memory describing it.
            if len(self.gap_detail) < 32:
                self.gap_detail.append((self.last_seq, seq))
        self.last_seq = seq

    @property
    def clean(self) -> bool:
        return self.gaps == 0 and self.out_of_order == 0 and self.blocks > 0

    def summary(self) -> str:
        if not self.blocks:
            return "no blocks read"
        span = f"{self.first_seq} to {self.last_seq}"
        if self.clean:
            return f"{self.blocks} blocks, sequence {span}, gaps 0"
        parts = [f"{self.blocks} blocks", f"sequence {span}",
                 f"gaps {self.gaps}", f"missing {self.missing}"]
        if self.out_of_order:
            parts.append(f"out of order {self.out_of_order}")
        if self.gap_detail:
            shown = ", ".join(f"{a}->{b}" for a, b in self.gap_detail[:4])
            parts.append(f"first gaps {shown}")
        return ", ".join(parts)
