#!/usr/bin/env python3
"""Negative tests for the bill of materials checker.

A rule engine that has only ever been shown a correct file has not been tested.
Each case here breaks inventory.json in exactly one way and asserts that the
checker names the fault. Five of the seven faults are pairings the front matter
already lists as errors found in the 198-page blueprint, so the test is not an
invention: it is the book's own list of what not to build, asked of a machine.

The last case asserts the real file is clean, because a checker that rejected
everything would pass all the others and be worthless.

    python tests/test_inventory.py

No framework and no device, the same as the lab's own suite.
"""
import copy
import json
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
import lint  # noqa: E402

SOURCE = json.loads((ROOT / "inventory.json").read_text(encoding="utf-8"))


def run(data):
    with tempfile.TemporaryDirectory() as d:
        path = Path(d) / "inventory.json"
        path.write_text(json.dumps(data), encoding="utf-8")
        return lint.check_inventory(path)


def nanopi_takes_a_hat(d):
    """Draft P4 and P12: a 40-pin HAT on the NEO Air's 24-pin header."""
    d["labs"]["P17"]["seated"] = {"nanopi": ["sim7020e"]}
    d["parts"]["sim7020e"]["labs"].append("P17")
    return "nanopi-24 header"


def two_hats_on_one_pi(d):
    """Draft P18: Explorer700 and SIM7600 on one Pi. Two boards, one header."""
    d["labs"]["P01"]["seated"]["pi4"].append("explorer700")
    d["parts"]["explorer700"]["labs"].append("P01")
    return "each own its header"


def modem_stacked_on_the_nucleo(d):
    """Draft P3: the SIM7070 treated as an Arduino shield."""
    d["labs"]["P03"]["hosts"].append("nucleo")
    d["labs"]["P03"]["seated"]["nucleo"] = ["sim7070g"]
    return "arduino header"


def two_instruments_in_one_lab(d):
    """Rule 5: the six instruments are never interchangeable, and never paired."""
    d["labs"]["P07"]["attached"].append("iks5a1")
    d["parts"]["iks5a1"]["labs"].append("P07")
    return "of the six instruments in one lab"


def five_volts_on_a_header(d):
    """Rule 4: every host pin is 3.3 V, whatever a board says it wants."""
    d["parts"]["mcc118"]["logic_volts"] = 5.0
    return "V logic on"


def a_part_that_forgot_a_lab(d):
    """The two halves of the file disagreeing, which is how such a file rots."""
    d["parts"]["sim7600e_h"]["labs"].remove("P13")
    return "does not name"


def a_lab_that_vanished(d):
    """Twenty labs, and the file must carry all twenty."""
    del d["labs"]["P12"]
    return "no entry for lab P12"


CASES = [nanopi_takes_a_hat, two_hats_on_one_pi, modem_stacked_on_the_nucleo,
         two_instruments_in_one_lab, five_volts_on_a_header,
         a_part_that_forgot_a_lab, a_lab_that_vanished]


def main():
    failures = 0

    clean = run(copy.deepcopy(SOURCE))
    if clean:
        print(f"FAIL  the real inventory.json is not clean: {clean}")
        failures += 1
    else:
        print("ok    the real inventory.json is clean")

    for case in CASES:
        data = copy.deepcopy(SOURCE)
        expected = case(data)
        problems = run(data)
        hit = [p for p in problems if expected in p]
        if hit:
            print(f"ok    {case.__name__}: {hit[0]}")
        else:
            print(f"FAIL  {case.__name__}: nothing said {expected!r}; got {problems}")
            failures += 1

    print(f"\n{len(CASES) + 1} cases, {failures} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
