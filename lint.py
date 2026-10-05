#!/usr/bin/env python3
"""House-style linter for the section sources and the bill of materials.

    python lint.py                 check every sections/*.tex and inventory.json
    python lint.py sections/p07.tex

Checks prose (everything outside verbatim code environments) for: em and en
dashes and LaTeX -- / ---, non-ASCII characters, violent idioms, and the
required subsection skeleton.  Checks code blocks for non-ASCII and for lines
longer than the page can print.

The tooling-attribution scan is deliberately NOT here.  A rule that spells the
words it forbids would put those words into the repository it protects, which
is the opposite of what it is for, so that scan lives outside this repository
and is run by hand before anything is published.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
VERB = re.compile(r"\\begin\{(asciiart|ccode|shellcode|pycode|makecode|dtscode|yamlcode"
                  r"|plaincode|lstlisting|verbatim)\}(.*?)\\end\{\1\}", re.S)
DASH = re.compile(r"(.{0,40})(\u2014|\u2013|(?<![-\w])---?(?![-\w>]))(.{0,40})")
VIOLENT = re.compile(r"\b(kill(?:s|ed|ing)?|attack(?:s|ed|ing)?|fight(?:s|ing)?|blame[ds]?"
                     r"|hurt(?:s|ing)?|destroy(?:s|ed|ing)?|suicid\w*|war against)\b", re.I)
REQUIRED = [r"\begin{unique}", "Intent", r"\begin{kit}", "System architecture",
            "Wiring and schematic", "Bench layout", "Software design", "Data flow",
            "Steps", "Acceptance test", "Practices", "Pitfalls", "Sources"]
MAXLEN = {"asciiart": 112}   # scriptsize fits ~115 columns
MAXCODE = 99                 # footnotesize at basewidth 0.48em fits ~99 columns


def check(path):
    s = path.read_text(encoding="utf-8")
    problems = []
    codes = [(m.group(1), m.group(2)) for m in VERB.finditer(s)]
    prose = VERB.sub("\n", s)

    for m in DASH.finditer(prose):
        problems.append(f"dash: ...{m.group(1)}[{m.group(2)}]{m.group(3)}...".replace("\n", " "))
    bad = sorted({c for c in prose if ord(c) > 126})
    if bad:
        problems.append(f"non-ASCII in prose: {bad}")
    for m in VIOLENT.finditer(prose):
        problems.append(f"violent idiom: {m.group(0)!r} near {prose[max(0,m.start()-40):m.end()+40]!r}")

    for env, code in codes:
        bad = sorted({c for c in code if ord(c) > 126})
        if bad:
            problems.append(f"non-ASCII in {env} block: {bad}")
        limit = MAXLEN.get(env, MAXCODE)
        for ln in code.splitlines():
            if len(ln) > limit:
                problems.append(f"{env} line {len(ln)} chars (max {limit}): {ln[:50]}...")

    if path.stem.startswith("p") and path.stem[1:].isdigit():
        for h in REQUIRED:
            if h not in s:
                problems.append(f"missing subsection: {h}")
        for fig in ("arch", "schematic", "bench", "uml"):
            if f"{{{path.stem}_{fig}}}" not in s:
                problems.append(f"missing figure: {path.stem}_{fig}")
    return problems


def check_inventory(path):
    """Rules 2, 3, 4 and 5 of the front matter, as arithmetic over inventory.json.

    What this can decide: that a board fits the header it is seated on, that no
    host carries two boards which each claim its header, that logic voltages
    match, that one lab holds at most one of the six instruments, and that the
    parts block and the labs block tell the same story.

    What it cannot decide is anything spatial. P19 is the standing example: the
    panel claims the header once and the sensor claims nothing, so this passes,
    and the lab is still blocked because the panel physically sits over the pins
    the sensor needs. A rule engine is not a bench.
    """
    data = json.loads(path.read_text(encoding="utf-8"))
    hosts, parts, labs = data["hosts"], data["parts"], data["labs"]
    problems = []
    used = {}

    for n in range(1, 21):
        if f"P{n:02d}" not in labs:
            problems.append(f"no entry for lab P{n:02d}")
    for lab in sorted(labs):
        if not re.fullmatch(r"P(0[1-9]|1[0-9]|20)", lab):
            problems.append(f"{lab} is not one of P01 to P20")

    for lab, spec in sorted(labs.items()):
        permitted = spec["hosts"]
        for h in permitted:
            if h not in hosts:
                problems.append(f"{lab}: unknown host {h!r}")
        for host, seated in sorted(spec["seated"].items()):
            if host not in permitted:
                problems.append(f"{lab}: seats parts on {host!r}, which its hosts list does not permit")
            spec_host = hosts.get(host)
            claimers = []
            for pid in seated:
                part = parts.get(pid)
                if part is None:
                    problems.append(f"{lab}: unknown part {pid!r} seated on {host}")
                    continue
                used.setdefault(pid, set()).add(lab)
                if spec_host is not None:
                    if part["fits"] != spec_host["header"]:
                        problems.append(
                            f"{lab}: {pid} fits the {part['fits']} header, and {host} has "
                            f"the {spec_host['header']} header")
                    if part["logic_volts"] != spec_host["logic_volts"]:
                        problems.append(
                            f"{lab}: {pid} is {part['logic_volts']} V logic on {host}, whose "
                            f"header is {spec_host['logic_volts']} V")
                if part["claims_header"]:
                    claimers.append(pid)
            if len(claimers) > 1:
                problems.append(
                    f"{lab}: {host} carries {len(claimers)} boards that each own its header, "
                    f"and the rule is fit one: {', '.join(sorted(claimers))}")
        for pid in spec["attached"]:
            if pid not in parts:
                problems.append(f"{lab}: unknown part {pid!r} attached")
                continue
            used.setdefault(pid, set()).add(lab)

        fitted = [p for s in spec["seated"].values() for p in s] + spec["attached"]
        instruments = sorted(p for p in fitted if parts.get(p, {}).get("instrument"))
        if len(instruments) > 1:
            problems.append(
                f"{lab}: {len(instruments)} of the six instruments in one lab, and the rule is "
                f"one: {', '.join(instruments)}")

    for pid, part in sorted(parts.items()):
        declared, actual = set(part["labs"]), used.get(pid, set())
        for lab in sorted(actual - declared):
            problems.append(f"{pid}: used by {lab}, which its own labs list does not name")
        for lab in sorted(declared - actual):
            problems.append(f"{pid}: claims {lab} uses it, and that lab does not list it")
    return problems


def main(argv):
    files = [Path(a) if Path(a).is_absolute() else ROOT / a for a in argv] or \
            sorted((ROOT / "sections").glob("*.tex")) + [ROOT / "inventory.json"]
    total = 0
    for f in files:
        pr = check_inventory(f) if f.suffix == ".json" else check(f)
        total += len(pr)
        if pr:
            print(f"== {f.name}: {len(pr)} problems")
            for p in pr[:25]:
                print("   " + p)
            if len(pr) > 25:
                print(f"   ... and {len(pr)-25} more")
        else:
            print(f"== {f.name}: clean")
    print("TOTAL PROBLEMS:", total)
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
