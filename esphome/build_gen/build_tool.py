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
        return subprocess.run(
            [ar, "rc", archive, f"@{rspfile}"], check=False
        ).returncode
    if mode == "copy":
        src, dst = sys.argv[2:4]
        shutil.copyfile(src, dst)
        return 0
    print(f"unknown build_tool mode: {mode}", file=sys.stderr)
    return 1


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
