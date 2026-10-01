#!/usr/bin/env python3
"""House-style linter for the section sources.

    python lint.py                 check every sections/*.tex
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


def main(argv):
    files = [Path(a) if Path(a).is_absolute() else ROOT / a for a in argv] or \
            sorted((ROOT / "sections").glob("*.tex"))
    total = 0
    for f in files:
        pr = check(f)
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
