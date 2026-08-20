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
        # Expand the response file here instead of passing @rspfile: GNU ar
        # treats backslashes in response files as escapes, corrupting Windows
        # paths ("sub\a.o" -> "suba.o").
        # One path per line (rspfile_content = $in_newline, written without
        # escaping), so a path containing a space survives. Expanding into
        # argv trades away the OS command-line length limit rspfiles dodge;
        # the relative object paths used here stay far below it.
        objects = Path(rspfile).read_text(encoding="utf-8").splitlines()
        return subprocess.run(
            [ar, "rc", archive, *filter(None, objects)], check=False, close_fds=False
        ).returncode
    if mode == "copy":
        src, dst = sys.argv[2:4]
        shutil.copyfile(src, dst)
        return 0
    print(f"unknown build_tool mode: {mode}", file=sys.stderr)
    return 1


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
