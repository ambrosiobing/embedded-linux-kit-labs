"""A physical bound on a resting probe, and an honest account of its reach.

The bench rule is to prefer a validity invariant to a plausibility check. For
an accelerometer at rest the magnitude of the three axes must be one g, and
that single number validates the register map, the byte order, the sign
convention and the full-scale setting at once. A per-axis plot cannot do that,
because a sample with the bytes swapped still looks like a number.

What is available here is the weaker, one-axis form, and saying exactly how
much weaker is part of the check rather than an apology for it. This lab's
stream carries one vibration channel, so the strongest statement is about the
static component: the sensor is direct coupled, so a channel at rest reads the
projection of gravity on its axis, which lies between minus one and plus one g.

**What this catches.** A scale wrong by a factor large enough to push the level
outside that band, which is the common case: a full-scale setting of plus or
minus 2 g read as plus or minus 16 g is out by eight.

**What it does not catch, measured rather than assumed.** A byte swap. On a
capture from this bench's own generator, an honest resting level of +0.984 g
became -0.416 g with the bytes exchanged, and both sit inside the bound, so the
check passes a stream it should refuse. The equality that does catch byte order
is the three-axis magnitude, because a swap that leaves one axis plausible will
not leave the sum of three squares at one g. One axis admits a bound and only
three axes admit an equality, and a bound cannot do an equality's work.

So this is a floor, not the invariant the bench rule asks for. Move to the
three-axis magnitude when the stream carries three axes, and until then do not
read a pass here as evidence that the byte order is right.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

# A probe at rest cannot see more than one g of static acceleration. The margin
# covers calibration error and a probe that is not perfectly still, and it is
# deliberately not generous: the faults this catches are off by a factor of two
# or more, not by a few per cent.
STATIC_LIMIT_G = 1.05


@dataclass(frozen=True)
class GravityCheck:
    """What the static level says about the scale and the byte order."""

    level_g: float
    spread_g: float
    blocks: int
    ok: bool
    reason: str


def dc_level_g(block: np.ndarray, g_per_count: float) -> float:
    """The static component of one block, in g."""
    return float(np.mean(np.asarray(block, dtype=np.float64))) * g_per_count


def check_at_rest(blocks, g_per_count: float | None, limit_g: float = STATIC_LIMIT_G
                  ) -> GravityCheck:
    """Run the one-axis invariant over an idle capture.

    `blocks` is an iterable of vibration channels. The caller supplies them
    because this is a statement about the accelerometer, not about the
    microphone, and nothing here should have to know which is which.
    """
    if g_per_count is None:
        return GravityCheck(0.0, 0.0, 0, True,
                            "no scale in the lab book, so the static level stays in counts "
                            "and this invariant cannot be applied. Record the full-scale "
                            "setting and its counts per g to turn it on.")
    levels = [dc_level_g(b, g_per_count) for b in blocks]
    if not levels:
        return GravityCheck(0.0, 0.0, 0, False, "no blocks, so nothing was checked")
    level = float(np.mean(levels))
    spread = float(np.std(levels))
    if abs(level) > limit_g:
        return GravityCheck(
            level, spread, len(levels), False,
            f"static level {level:+.3f} g on a probe said to be at rest, and no axis can "
            f"see more than {limit_g:.2f} g of gravity. The counts are not being read as "
            "the data sheet describes. The usual causes are a full-scale setting that "
            "disagrees with the lab book and a byte order that disagrees with the part.")
    return GravityCheck(
        level, spread, len(levels), True,
        f"static level {level:+.3f} g over {len(levels)} blocks, spread {spread:.3f} g, "
        f"within the {limit_g:.2f} g that gravity allows on one axis. This is the one-axis "
        "bound; the three-axis magnitude is the stronger test and needs three axes.")
