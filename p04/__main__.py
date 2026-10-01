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
from benchkit.frames import Transport
from benchkit.invariants import check_at_rest
from benchkit.sources import read_capture, read_serial, write_capture
from .synth import G_PER_COUNT, stream

DEFAULT_RATE = 26667.0        # IIS3DWB output data rate, confirm against the firmware
DEFAULT_BLOCK = 2048          # the chapter's block length


def cmd_synth(a) -> int:
    drop = tuple(int(s) for s in a.drop.split(",") if s.strip()) if a.drop else ()
    blocks = stream(a.kind, a.blocks, a.block_len, a.rate, a.seed, drop=drop,
                    stamp=not a.no_stamp, true_rate_hz=a.true_rate)
    meta = {"kind": a.kind, "block_len": a.block_len, "sample_rate_hz": a.rate,
            "seed": a.seed, "synthetic": True,
            "g_per_count": G_PER_COUNT,
            "warning": "synthetic stimulus, not a measurement of any hardware"}
    if a.true_rate is not None:
        meta["true_rate_hz"] = a.true_rate
    n = write_capture(a.out, blocks, meta)
    print(f"wrote {n} blocks of {a.kind} to {a.out}")
    if drop:
        print(f"deliberately omitted sequence numbers: {', '.join(str(d) for d in drop)}")
    return 0


def cmd_baseline(a) -> int:
    blocks, meta = read_capture(a.source)
    rate = a.rate if a.rate is not None else meta.get("sample_rate_hz")
    mean, count, transport, timing = measure_baseline(blocks, rate, a.band_low, a.band_high)
    print(f"transport: {transport.summary()}")
    if not transport.clean:
        print("refusing to take a baseline from a stream with holes in it.")
        return 1
    if timing is not None:
        ok, why = timing.check(rate, a.rate_tolerance)
        print(f"rate: {why}")
        if not ok:
            print("refusing to write a lab book whose band edges would be wrong.")
            return 1
    book = LabBook(baseline=mean, warn=a.warn, fault=a.fault, mounting=a.mounting,
                   block_len=int(meta.get("block_len", DEFAULT_BLOCK)),
                   sample_rate_hz=rate, band_low_hz=a.band_low, band_high_hz=a.band_high,
                   g_per_count=a.g_per_count if a.g_per_count is not None
                   else meta.get("g_per_count"),
                   rate_tolerance=a.rate_tolerance,
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
    rate_ok = True
    if result.timing is not None:
        rate_ok, why = result.timing.check(book.sample_rate_hz, book.rate_tolerance)
        print(f"rate: {why}")
    if not result.transport.clean:
        print("the spectrum below was computed over a stream with holes in it")
    if not rate_ok:
        print("refusing to report a band energy against a frequency axis that is wrong.")
        return 1
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


def cmd_validate(a) -> int:
    """Ask physics whether the counts are being read as the data sheet describes.

    Separate from analyse on purpose. A spectrum can look entirely reasonable
    while the full-scale setting or the byte order is wrong, because a swapped
    sample is still a number. This is the question a plot cannot answer.
    """
    book = LabBook.load(a.book)
    blocks, _ = read_capture(a.source)
    vib = []
    transport = Transport()
    for b in blocks:
        transport.observe(b.seq)
        vib.append(b.vib)
    print(f"transport: {transport.summary()}")
    check = check_at_rest(vib, book.g_per_count)
    print(f"at rest: {check.reason}")
    return 0 if check.ok else 1


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
    s.add_argument("--no-stamp", action="store_true", dest="no_stamp",
                   help="write a capture with no clock, as a probe whose firmware "
                        "exposes none would produce")
    s.add_argument("--true-rate", type=float, default=None, dest="true_rate",
                   help="the rate the timestamps really advance at, when it is to "
                        "differ from the rate a lab book would claim")
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
    b.add_argument("--g-per-count", type=float, default=None, dest="g_per_count",
                   help="counts per g from the full-scale setting actually programmed; "
                        "without it the gravity invariant cannot run")
    b.add_argument("--rate-tolerance", type=float, default=0.02, dest="rate_tolerance",
                   help="how far the observed rate may sit from the claimed one")
    b.add_argument("--note", default="")
    b.add_argument("--out", required=True)
    b.set_defaults(func=cmd_baseline)

    for name, fn, helptext in (("analyse", cmd_analyse, "classify a capture"),
                               ("replay", cmd_replay, "run a capture twice and compare")):
        c = sub.add_parser(name, help=helptext)
        c.add_argument("--source", required=True)
        c.add_argument("--book", required=True)
        c.set_defaults(func=fn)

    v = sub.add_parser("validate", help="the gravity invariant over a capture at rest")
    v.add_argument("--source", required=True)
    v.add_argument("--book", required=True)
    v.set_defaults(func=cmd_validate)

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
