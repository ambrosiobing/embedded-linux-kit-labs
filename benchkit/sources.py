"""Where blocks come from, and the one source that is deliberately not written.

The chapter is explicit: if the firmware is the Zephyr text dashboard, parse
lines; if it is HSDatalog binary, use ST's host decoder first, and do not
invent a frame format. That instruction is a design constraint, not advice,
and it is why this module has a text reader and a capture reader and refuses
to guess at a binary one.

Everything downstream takes an iterable of Block, so the analysis does not know
or care whether a block came from a probe, a file or a generator. That is what
makes the whole pipeline runnable, testable and demonstrable with no hardware
on the desk, and it is the same property that later lets a recorded capture be
replayed through a changed analysis without touching the firmware.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Iterable, Iterator

import numpy as np

from .frames import Block

CAPTURE_VERSION = 1


class FrameFormatUnknown(NotImplementedError):
    """Raised for the binary path, with the reason rather than a blank refusal."""


def read_capture(path: str | Path) -> tuple[Iterator[Block], dict]:
    """Blocks from a capture written by write_capture.

    The container is numpy's npz, which is self describing and needs no parser
    here. It is this project's own format for recorded and synthetic runs, and
    it makes no claim about what the probe puts on the wire.
    """
    with np.load(Path(path), allow_pickle=False) as z:
        seq = z["seq"]
        vib = z["vib"]
        mic = z["mic"]
        meta = json.loads(str(z["meta"])) if "meta" in z else {}
    if not (len(seq) == len(vib) == len(mic)):
        raise ValueError(f"{path}: seq, vib and mic differ in length")

    def gen() -> Iterator[Block]:
        for i in range(len(seq)):
            yield Block(int(seq[i]), vib[i], mic[i])

    return gen(), meta


def write_capture(path: str | Path, blocks: Iterable[Block], meta: dict | None = None) -> int:
    """Write blocks to a capture. Returns how many were written."""
    seq, vib, mic = [], [], []
    for b in blocks:
        seq.append(b.seq)
        vib.append(np.asarray(b.vib, dtype=np.int16))
        mic.append(np.asarray(b.mic, dtype=np.int16))
    if not seq:
        raise ValueError("refusing to write an empty capture")
    payload = dict(meta or {})
    payload.setdefault("capture_version", CAPTURE_VERSION)
    np.savez_compressed(Path(path), seq=np.asarray(seq, dtype=np.int64),
                        vib=np.stack(vib), mic=np.stack(mic),
                        meta=np.asarray(json.dumps(payload)))
    return len(seq)


def read_text_lines(lines: Iterable[str], block_len: int) -> Iterator[Block]:
    """Blocks from a line oriented dashboard firmware.

    The accepted line is `seq,vib0,...,vibN,|,mic0,...,micN`, which is this
    project's reading of a text dashboard rather than a specification of one.
    The real separator, field order and sample count come from the firmware
    that is actually flashed, and this function is the single place to change
    when that is known. A malformed line is skipped and counted by the caller
    rather than terminating the run, because one bad line on a serial link is
    normal and a stopped gateway is not.
    """
    for raw in lines:
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(",")
        if "|" not in parts:
            continue
        split = parts.index("|")
        try:
            seq = int(parts[0])
            vib = np.asarray([int(v) for v in parts[1:split]], dtype=np.int16)
            mic = np.asarray([int(v) for v in parts[split + 1:]], dtype=np.int16)
        except ValueError:
            continue
        if len(vib) != block_len or len(mic) != block_len:
            continue
        yield Block(seq, vib, mic)


def read_serial(port: str, baud: int, block_len: int) -> Iterator[Block]:
    """Blocks from a probe, over the text protocol.

    pyserial is imported here rather than at module level on purpose. The
    authoring laptop does not have it, every other source works without it, and
    a missing dependency should stop the one command that needs it rather than
    the whole package.
    """
    try:
        import serial  # noqa: PLC0415
    except ImportError as exc:
        raise SystemExit(
            "this source needs pyserial: python -m pip install pyserial") from exc
    with serial.Serial(port, baud, timeout=2) as ser:
        def lines() -> Iterator[str]:
            while True:
                raw = ser.readline()
                if not raw:
                    return
                yield raw.decode("ascii", errors="ignore")
        yield from read_text_lines(lines(), block_len)


def read_binary(path: str | Path) -> Iterator[Block]:
    """The HSDatalog path, which is not implemented and should not be guessed.

    A raw capture of the CDC endpoint is bytes in a layout that belongs to the
    firmware. Decoding it by inspection produces a reader that works on one
    build and silently misreads the next, and the chapter says so directly.
    Run ST's host decoder, write the result with write_capture, and the rest of
    this pipeline accepts it unchanged.
    """
    raise FrameFormatUnknown(
        f"{path}: this is a raw byte capture, and its layout belongs to the firmware. "
        "Decode it with ST's HSDatalog host tool, then write the result as an npz "
        "capture with p04.sources.write_capture. Do not infer the layout here.")
