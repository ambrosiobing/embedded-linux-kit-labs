"""The baseline, the two thresholds and the mounting, kept in one file.

The chapter refuses to prescribe the thresholds and gives the reason: they are
measured on the bench, against a baseline captured on an idle machine, with the
mounting written down beside them, because a threshold without its baseline is
not a measurement. This module is that rule made structural. There is no
default baseline and no default threshold anywhere in this package, and the
classifier cannot be constructed without both.

A lab book entry is also the unit of repeatability. Two runs a week apart are
comparable when they name the same file; they are not comparable because they
happen to print the same word.
"""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path

from .analysis import Thresholds


@dataclass
class LabBook:
    """What a run needs to be repeatable, and nothing else."""

    baseline: float
    warn: float
    fault: float
    mounting: str
    block_len: int
    sample_rate_hz: float | None = None
    band_low_hz: float | None = None
    band_high_hz: float | None = None
    source: str = ""
    captured: str = field(default_factory=lambda: datetime.now(timezone.utc)
                          .strftime("%A %d %B %Y, %H:%M UTC"))
    note: str = ""

    def thresholds(self) -> Thresholds:
        return Thresholds(self.baseline, self.warn, self.fault)

    def save(self, path: str | Path) -> Path:
        p = Path(path)
        p.write_text(json.dumps(asdict(self), indent=2) + "\n", encoding="utf-8")
        return p

    @classmethod
    def load(cls, path: str | Path) -> "LabBook":
        p = Path(path)
        if not p.exists():
            raise SystemExit(
                f"no lab book at {p}. Capture one first:\n"
                f"    python -m p04 baseline --source <capture.npz> --mounting '<how it is held>' "
                f"--out {p}\n"
                "There is no default baseline, because a threshold without its baseline "
                "is not a measurement.")
        data = json.loads(p.read_text(encoding="utf-8"))
        known = {f for f in cls.__dataclass_fields__}
        unknown = set(data) - known
        if unknown:
            raise SystemExit(f"{p}: unknown fields {sorted(unknown)}")
        return cls(**data)

    def describe(self) -> str:
        band = "upper half of the spectrum"
        if self.sample_rate_hz and self.band_low_hz:
            hi = self.band_high_hz or (self.sample_rate_hz / 2.0)
            band = f"{self.band_low_hz:.0f} to {hi:.0f} Hz"
        return (f"baseline {self.baseline:.1f}, warn {self.warn:.2f}, fault {self.fault:.2f}\n"
                f"band {band}, block {self.block_len} samples\n"
                f"mounting: {self.mounting}\n"
                f"captured {self.captured} from {self.source or 'an unnamed source'}")
