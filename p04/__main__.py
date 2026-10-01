"""The command line for lab P04.

    python -m p04 synth    --kind touch --out touch.npz
    python -m p04 baseline --source idle.npz --mounting "..." --out labbook.json
    python -m p04 analyse  --source touch.npz --book labbook.json
    python -m p04 replay   --source touch.npz --book labbook.json
    python -m p04 transport --source touch.npz
    python -m p04 live     --port /dev/ttyACM0 --book labbook.json

Every command but `live` runs with no hardware. `live` is the only one that
needs a probe, and it is the only one that needs pyserial.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from benchkit.labbook import LabBook
from .pipeline import measure_baseline, run_with
from benchkit.sources import read_capture, read_serial, write_capture
from .synth import stream

DEFAULT_RATE = 26667.0        # IIS3DWB output data rate, confirm against the firmware
DEFAULT_BLOCK = 2048          # the chapter's block length


def cmd_synth(a) -> int:
    drop = tuple(int(s) for s in a.drop.split(",") if s.strip()) if a.drop else ()
    blocks = stream(a.kind, a.blocks, a.block_len, a.rate, a.seed, drop=drop)
    meta = {"kind": a.kind, "block_len": a.block_len, "sample_rate_hz": a.rate,
            "seed": a.seed, "synthetic": True,
            "warning": "synthetic stimulus, not a measurement of any hardware"}
    n = write_capture(a.out, blocks, meta)
    print(f"wrote {n} blocks of {a.kind} to {a.out}")
    if drop:
        print(f"deliberately omitted sequence numbers: {', '.join(str(d) for d in drop)}")
    return 0


def cmd_baseline(a) -> int:
    blocks, meta = read_capture(a.source)
    rate = a.rate if a.rate is not None else meta.get("sample_rate_hz")
    mean, count, transport = measure_baseline(blocks, rate, a.band_low, a.band_high)
    print(f"transport: {transport.summary()}")
    if not transport.clean:
        print("refusing to take a baseline from a stream with holes in it.")
        return 1
    book = LabBook(baseline=mean, warn=a.warn, fault=a.fault, mounting=a.mounting,
                   block_len=int(meta.get("block_len", DEFAULT_BLOCK)),
                   sample_rate_hz=rate, band_low_hz=a.band_low, band_high_hz=a.band_high,
                   source=str(a.source), note=a.note)
    book.save(a.out)
    print(f"baseline {mean:.1f} over {count} blocks, written to {a.out}")
    print(book.describe())
    if meta.get("synthetic"):
        print("\nNote: this baseline came from a synthetic capture. It is good enough to\n"
              "exercise the pipeline and it is not a bench measurement. Recapture on the\n"
              "probe before any number from this lab is quoted.")
    return 0


def _analyse(a, label: str) -> int:
    book = LabBook.load(a.book)
    blocks, _ = read_capture(a.source)
    result = run_with(blocks, book)
    print(f"transport: {result.transport.summary()}")
    if not result.transport.clean:
        print("the spectrum below was computed over a stream with holes in it")
    worst = result.worst()
    print(f"{label}: {worst}, mean ratio {result.mean_ratio():.2f}, "
          f"mean microphone RMS {result.mean_mic_rms():.1f}")
    counts = {s: result.states.count(s) for s in ("NORMAL", "WARNING", "FAULT")}
    print("blocks: " + ", ".join(f"{k} {v}" for k, v in counts.items() if v))
    return 0 if result.transport.clean else 1


def cmd_analyse(a) -> int:
    return _analyse(a, "worst state")


def cmd_replay(a) -> int:
    """Run the same capture twice and require the two runs to agree.

    This is an acceptance criterion of the lab rather than a convenience: if a
    replay of one file disagrees with itself, the pipeline is carrying state it
    should not have, and every number it has ever printed is in question.
    """
    book = LabBook.load(a.book)
    first, _ = read_capture(a.source)
    second, _ = read_capture(a.source)
    r1 = run_with(first, book)
    r2 = run_with(second, book)
    same = r1.states == r2.states
    print(f"transport: {r1.transport.summary()}")
    print(f"first pass:  {r1.worst()}, mean ratio {r1.mean_ratio():.4f}")
    print(f"second pass: {r2.worst()}, mean ratio {r2.mean_ratio():.4f}")
    if same and r1.mean_ratio() == r2.mean_ratio():
        print("the two passes agree exactly, so the pipeline holds no state")
        return 0
    print("THE TWO PASSES DISAGREE. The pipeline has state it should not have.")
    return 1


def cmd_transport(a) -> int:
    from .pipeline import Transport
    blocks, _ = read_capture(a.source)
    t = Transport()
    for b in blocks:
        t.observe(b.seq)
    print(t.summary())
    return 0 if t.clean else 1


def cmd_live(a) -> int:
    book = LabBook.load(a.book)
    blocks = read_serial(a.port, a.baud, book.block_len)
    result = run_with(blocks, book)
    print(f"transport: {result.transport.summary()}")
    print(f"worst state: {result.worst()}, mean ratio {result.mean_ratio():.2f}")
    return 0


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="p04", description=__doc__.splitlines()[0])
    sub = p.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("synth", help="write a synthetic capture, no hardware")
    s.add_argument("--kind", default="idle", choices=("idle", "touch", "speech"))
    s.add_argument("--blocks", type=int, default=16)
    s.add_argument("--block-len", type=int, default=DEFAULT_BLOCK, dest="block_len")
    s.add_argument("--rate", type=float, default=DEFAULT_RATE)
    s.add_argument("--seed", type=int, default=0)
    s.add_argument("--drop", default="", help="sequence numbers to omit, comma separated")
    s.add_argument("--out", required=True)
    s.set_defaults(func=cmd_synth)

    b = sub.add_parser("baseline", help="measure a baseline and write a lab book")
    b.add_argument("--source", required=True, help="an idle capture")
    b.add_argument("--mounting", required=True, help="how the probe is held, in words")
    b.add_argument("--warn", type=float, required=True, help="ratio above baseline")
    b.add_argument("--fault", type=float, required=True, help="ratio above baseline")
    b.add_argument("--rate", type=float, default=None)
    b.add_argument("--band-low", type=float, default=None, dest="band_low")
    b.add_argument("--band-high", type=float, default=None, dest="band_high")
    b.add_argument("--note", default="")
    b.add_argument("--out", required=True)
    b.set_defaults(func=cmd_baseline)

    for name, fn, helptext in (("analyse", cmd_analyse, "classify a capture"),
                               ("replay", cmd_replay, "run a capture twice and compare")):
        c = sub.add_parser(name, help=helptext)
        c.add_argument("--source", required=True)
        c.add_argument("--book", required=True)
        c.set_defaults(func=fn)

    t = sub.add_parser("transport", help="sequence numbers only, no spectrum")
    t.add_argument("--source", required=True)
    t.set_defaults(func=cmd_transport)

    l = sub.add_parser("live", help="read a probe over the text protocol")
    l.add_argument("--port", required=True)
    l.add_argument("--baud", type=int, default=921600)
    l.add_argument("--book", required=True)
    l.set_defaults(func=cmd_live)

    a = p.parse_args(argv)
    return a.func(a)


if __name__ == "__main__":
    sys.exit(main())
