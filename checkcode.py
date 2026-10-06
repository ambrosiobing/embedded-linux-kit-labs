#!/usr/bin/env python3
"""Syntax check for every command block the labs tell a reader to type.

    python checkcode.py                      check every sections/*.tex
    python checkcode.py --require-shellcheck fail if shellcheck is absent
    python checkcode.py sections/p05.tex     check one lab

The labs print shell and Python for a reader to run at a bench, and until this
existed nothing had ever parsed any of it. A block with an unbalanced quote or a
stray colon would have been published, read, typed, and found by the reader.

The blocks are not lifted into files. The prose is the only copy, which is the
point: an extracted script and the chapter it came from are two things that can
disagree, and the chapter is what a reader actually types from. This reads the
blocks where they live and checks them in memory.

Python is compiled, never executed, so an import of a library this machine does
not have is fine and a syntax error is not. Shell goes to shellcheck if it is on
PATH. Without it the shell half is skipped and said to be skipped, out loud, on
every run: a skipped check that announces itself quietly is how a check stops
existing. Continuous integration passes --require-shellcheck so that the skip can
never happen there.
"""
import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BLOCK = re.compile(r"\\begin\{(shellcode|pycode)\}\n(.*?)\\end\{\1\}", re.S)

# Rules that describe a script, not a line someone types at a prompt. A reader
# running one command at a time has a shell to come back to, so "cd x || exit"
# is advice for a file rather than for a bench. Each exclusion costs a class of
# real finding, so the list stays short and every entry says why it is here.
SHELLCHECK_EXCLUDE = [
    "SC2164",  # cd without || exit: typed interactively, the shell survives
    "SC2103",  # using cd ... && ( ... ) instead of a subshell: same reason
]


def blocks(path):
    text = path.read_text(encoding="utf-8")
    for m in BLOCK.finditer(text):
        line = text.count("\n", 0, m.start()) + 1
        yield m.group(1), m.group(2), line


def check_python(code, where):
    try:
        compile(code, where, "exec")
    except SyntaxError as e:
        return [f"{where}: python syntax: {e.msg} (block line {e.lineno})"]
    return []


def check_shell(code, where, exe):
    args = [exe, "--shell=bash", "--severity=warning", "--format=gcc",
            f"--exclude={','.join(SHELLCHECK_EXCLUDE)}", "-"]
    done = subprocess.run(args, input=code, capture_output=True, text=True)
    if done.returncode == 0:
        return []
    return [f"{where}: {ln.replace('-:', 'block line ', 1)}"
            for ln in done.stdout.splitlines() if ln.strip()]


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("files", nargs="*")
    ap.add_argument("--require-shellcheck", action="store_true",
                    help="fail rather than skip when shellcheck is not on PATH")
    args = ap.parse_args(argv)

    files = [Path(a) if Path(a).is_absolute() else ROOT / a for a in args.files] \
        or sorted((ROOT / "sections").glob("*.tex"))

    exe = shutil.which("shellcheck")
    if exe is None and args.require_shellcheck:
        print("shellcheck is not on PATH, and --require-shellcheck was given.")
        return 2

    problems, counted = [], {"shellcode": 0, "pycode": 0}
    for path in files:
        for kind, code, line in blocks(path):
            counted[kind] += 1
            where = f"{path.name}:{line}"
            if kind == "pycode":
                problems += check_python(code, where)
            elif exe is not None:
                problems += check_shell(code, where, exe)

    print(f"{counted['pycode']} Python blocks compiled")
    if exe is None:
        print(f"{counted['shellcode']} shell blocks NOT CHECKED: "
              f"shellcheck is not on PATH of this machine")
    else:
        print(f"{counted['shellcode']} shell blocks passed to shellcheck "
              f"(excluding {', '.join(SHELLCHECK_EXCLUDE)})")

    for p in problems:
        print("  " + p)
    print("TOTAL PROBLEMS:", len(problems))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
