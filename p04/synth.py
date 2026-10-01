"""Synthetic stimuli, so the lab can be rehearsed before the probe is wired.

These are not a model of the STWIN.box and they are not claimed to be. They are
three signals with the property the lab's acceptance test turns on, which is
that one of them moves the vibration band and not the microphone, and another
moves the microphone and not the vibration band. That property is enough to
build the pipeline, write its tests and fix its defects while the hardware is
still in its box.

What they cannot tell you is any number you would write in the lab book. The
baseline, the two thresholds and the mounting all have to come from the bench,
and the chapter says so. A threshold tuned against this generator is a
threshold tuned against this generator.

Every generator takes a seed and is deterministic, because a test that fails
one run in fifty teaches nothing.
"""

from __future__ import annotations

from typing import Iterator

import numpy as np

from benchkit.frames import Block

FULL_SCALE = 32767


def _noise(rng: np.random.Generator, n: int, level: float) -> np.ndarray:
    return rng.normal(0.0, level * FULL_SCALE, n)


def _clip16(x: np.ndarray) -> np.ndarray:
    return np.clip(np.rint(x), -FULL_SCALE, FULL_SCALE).astype(np.int16)


def idle(n: int, rng: np.random.Generator, vib_level: float = 0.004,
         mic_level: float = 0.004) -> tuple[np.ndarray, np.ndarray]:
    """A quiet machine: broadband noise on both channels and nothing else."""
    return _noise(rng, n, vib_level), _noise(rng, n, mic_level)


def case_touch(n: int, rng: np.random.Generator, sample_rate_hz: float,
               ring_hz: float = 4200.0, amplitude: float = 0.05,
               decay_s: float = 0.06, offset_s: float = 0.0
               ) -> tuple[np.ndarray, np.ndarray]:
    """A knuckle on the case: a damped ring in the vibration band.

    The microphone gets the idle floor and nothing more. A real touch is
    audible, so on the bench the two channels will not be as clean as this;
    that is the point of taking the orthogonality numbers on hardware rather
    than trusting these.

    `decay_s` defaults long enough that the ring spans most of a block. That is
    not cosmetic. A Hann window is near zero at both ends of the block, so a
    transient that lands at the start keeps only about a third of its weight,
    measured, and a shorter decay makes detection depend on where the tap falls
    relative to the block boundary rather than on how hard it was. `offset_s`
    exists so that effect can be tested rather than only described.
    """
    t = np.arange(n) / sample_rate_hz
    since = t - offset_s
    ring = np.where(since >= 0,
                    amplitude * FULL_SCALE * np.exp(-np.clip(since, 0, None) / decay_s)
                    * np.sin(2 * np.pi * ring_hz * np.clip(since, 0, None)),
                    0.0)
    vib, mic = idle(n, rng)
    return vib + ring, mic


def speech(n: int, rng: np.random.Generator, sample_rate_hz: float,
           fundamental_hz: float = 180.0, amplitude: float = 0.08
           ) -> tuple[np.ndarray, np.ndarray]:
    """A voice near the microphone: a low fundamental and two harmonics.

    The vibration channel gets the idle floor. Low frequency and on the other
    channel, which is what makes it the opposite stimulus to a case touch.
    """
    t = np.arange(n) / sample_rate_hz
    voice = np.zeros(n)
    for k, weight in ((1, 1.0), (2, 0.5), (3, 0.25)):
        voice += weight * np.sin(2 * np.pi * fundamental_hz * k * t)
    voice *= amplitude * FULL_SCALE / 1.75
    vib, mic = idle(n, rng)
    return vib, mic + voice


STIMULI = {"idle": idle, "touch": case_touch, "speech": speech}


def stream(kind: str, blocks: int, block_len: int = 2048,
           sample_rate_hz: float = 26667.0, seed: int = 0,
           first_seq: int = 1, drop: tuple[int, ...] = ()) -> Iterator[Block]:
    """A run of blocks of one kind.

    `drop` names sequence numbers to omit, which is how the transport tests
    produce a stream with holes in it without needing a loose cable.
    """
    if kind not in STIMULI:
        raise ValueError(f"unknown stimulus {kind!r}; try one of {sorted(STIMULI)}")
    rng = np.random.default_rng(seed)
    for i in range(blocks):
        seq = first_seq + i
        if seq in drop:
            continue
        if kind == "idle":
            vib, mic = idle(block_len, rng)
        else:
            vib, mic = STIMULI[kind](block_len, rng, sample_rate_hz)
        yield Block(seq, _clip16(vib), _clip16(mic))
