"""Tiny cross-platform build steps invoked from the generated ninja file.

Plain script (not ``python -m``): it runs from ninja with whatever Python
started esphome and must not depend on the package being importable.

Subcommands:
    ar <ar-binary> <archive> <rspfile>   remove stale archive, then ``ar rc``
    copy <src> <dst>                     copy a file
"""

from pathlib import Path
import shutil
import subprocess
import sys


def main() -> int:
    mode = sys.argv[1]
    if mode == "ar":
        ar, archive, rspfile = sys.argv[2:5]
        # Remove first: ``ar rc`` replaces members but never drops ones whose
        # source was removed from the build, which would leak stale objects.
        Path(archive).unlink(missing_ok=True)
        # GNU ar treats backslashes in response files as escapes (corrupts
        # Windows paths), so expand the rspfile into argv, stripping the
        # simple surrounding quote ninja adds to special paths.
        objects = [
            line[1:-1]
            if len(line) >= 2 and line[0] == line[-1] and line[0] in "'\""
            else line
            for line in Path(rspfile).read_text(encoding="utf-8").splitlines()
            if line
        ]
        if not objects:
            # An empty archive would "succeed" here and fail far away at link
            print(f"ar: no objects listed in {rspfile} for {archive}", file=sys.stderr)
            return 1
        return subprocess.run(
            [ar, "rc", archive, *objects], check=False, close_fds=False
        ).returncode
    if mode == "copy":
        src, dst = sys.argv[2:4]
        shutil.copyfile(src, dst)
        return 0
    print(f"unknown build_tool mode: {mode}", file=sys.stderr)
    return 1


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
