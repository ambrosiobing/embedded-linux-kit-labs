#!/usr/bin/env python3
"""Everything the workflow can no longer check, run here before you push.

    python prepublish.py

The sources are not published any more. Only the Markdown is, and `*.tex` is
ignored, so continuous integration cannot see `sections/` or `figures/*.tex` and
cannot verify that the Markdown still matches them. Four checks lost their input
that way, and they are the ones that prove the book agrees with its source:

  * the house rules over every section
  * the parser over every command block the labs print
  * the per-lab figure and lab-line existence check
  * the regeneration check, that chapters/, CONTENTS.md and PITFALLS.md are
    current rather than merely present

They did not stop mattering when the workflow stopped seeing them. They run here
instead, and this script refuses rather than warns, so that "I ran prepublish"
means the same thing a green run used to mean.

The order matters. Regeneration comes first, because a stale chapter is the
failure this volume actually had: CONTENTS.md was generated at the root for
weeks with nothing checking it. Everything after that reads what regeneration
produced.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def run(label, args, must_be_clean=False):
    print(f"\n=== {label}")
    done = subprocess.run(args, cwd=ROOT, capture_output=True, text=True)
    out = (done.stdout + done.stderr).strip()
    if out:
        print("\n".join(out.splitlines()[-12:]))
    if done.returncode != 0:
        print(f"FAILED: {label}")
        return False
    return True


def sources_present():
    """The sources are untracked now, so their absence is a real possibility."""
    missing = [p for p in ("sections", "figures", "main.tex") if not (ROOT / p).exists()]
    if missing:
        print(f"\nThe authoring sources are not in this clone: {', '.join(missing)}.")
        print("They are deliberately unpublished, so a fresh clone does not have")
        print("them. Copy them from the catalogue before publishing anything.")
        return False
    return True


def regenerated_cleanly():
    """mdbuild writes chapters/, CONTENTS.md and PITFALLS.md. They must not move."""
    paths = ["chapters", "CONTENTS.md", "PITFALLS.md"]
    done = subprocess.run(["git", "diff", "--quiet", "--"] + paths, cwd=ROOT)
    if done.returncode != 0:
        print("\nThe generated files moved when they were regenerated, which means")
        print("what is committed is behind the sources. Commit the regeneration:")
        subprocess.run(["git", "--no-pager", "diff", "--stat", "--"] + paths, cwd=ROOT)
        return False
    return True


def labs_complete():
    problems = []
    for n in range(1, 21):
        sec = ROOT / "sections" / f"p{n:02d}.tex"
        if not sec.exists():
            problems.append(f"missing sections/p{n:02d}.tex")
            continue
        if "project{" not in sec.read_text(encoding="utf-8"):
            problems.append(f"p{n:02d}: no lab line")
        for fig in ("arch", "schematic", "bench", "uml"):
            if not (ROOT / "figures" / f"p{n:02d}_{fig}.tex").exists():
                problems.append(f"missing figures/p{n:02d}_{fig}.tex")
    for extra in ("sections/front.tex", "sections/appendix.tex", "figures/front_map.tex"):
        if not (ROOT / extra).exists():
            problems.append(f"missing {extra}")
    for p in problems:
        print("  " + p)
    return not problems


def readmes_resolve():
    """Every link in every README points at something that exists, every chapter
    is reachable from the front page, and every lab directory with a Makefile has
    a README. A renamed chapter breaks all three at once and no other check here
    can see it: on Sunday 8 November 2026 the front page still pointed at
    chapters/09-nanopi-neo-air-headless-hub.md, so P09's chapter was published
    and linked from nowhere."""
    link = re.compile(r"\[([^\]]*)\]\(([^)\s]+)\)")
    pages = [ROOT / "README.md", ROOT / "journal" / "README.md"]
    pages += sorted(ROOT.glob("p[0-9][0-9]/README.md"))
    problems = []
    for page in pages:
        if not page.exists():
            problems.append("missing " + page.relative_to(ROOT).as_posix())
            continue
        for _, target in link.findall(page.read_text(encoding="utf-8")):
            if target.startswith(("http://", "https://", "#", "mailto:")):
                continue
            head = target.split("#", 1)[0]
            if head and not (page.parent / head).exists():
                problems.append(
                    page.relative_to(ROOT).as_posix() + ": dead link " + target)
    front = ""
    if (ROOT / "README.md").exists():
        front = (ROOT / "README.md").read_text(encoding="utf-8")
    for chapter in sorted((ROOT / "chapters").glob("*.md")):
        if chapter.name not in front:
            problems.append("chapters/" + chapter.name + ": linked from no page")
    for lab in sorted(ROOT.glob("p[0-9][0-9]")):
        if (lab / "Makefile").exists() and not (lab / "README.md").exists():
            problems.append(lab.name + "/: has a Makefile and no README.md")
    for problem in problems:
        print("  " + problem)
    return not problems


def main():
    if not sources_present():
        return 2

    ok = True
    ok &= run("Regenerating the Markdown edition", [sys.executable, "mdbuild.py"])
    print("\n=== The generated files are current")
    ok &= regenerated_cleanly()
    print("\n=== Every lab has its four figures and a lab line")
    ok &= labs_complete()
    print("\n=== Every README resolves, and no chapter is orphaned")
    ok &= readmes_resolve()
    ok &= run("House rules over every lab, and the bill of materials",
              [sys.executable, "lint.py"])
    ok &= run("Every command the labs print is parsed",
              [sys.executable, "checkcode.py"])
    ok &= run("The bill of materials refuses the rejected pairings",
              [sys.executable, "tests/test_inventory.py"])

    import shutil
    no_shellcheck = shutil.which("shellcheck") is None

    print()
    if no_shellcheck:
        print("prepublish: WARNING. shellcheck is not on this machine, so the")
        print("shell blocks counted above were not checked by anything, and the")
        print("count is printed by the run rather than written here, because a")
        print("number in a sentence is a number nothing updates. Continuous")
        print("integration used to check them and can no longer see the")
        print("sources, so this is now the only place they could be checked.")
        print("Run this script on a machine with shellcheck before a release,")
        print("or accept the gap knowingly rather than by default.")
    if ok:
        print("prepublish: all checks that could run passed."
              if no_shellcheck else "prepublish: all checks passed.")
        print("Safe to commit and push.")
        return 0
    print("prepublish: FAILED. Do not push until the above is fixed.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
