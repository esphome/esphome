"""Tiny cross-platform build steps invoked from the generated ninja file.

Plain script (not ``python -m``): it runs from ninja with whatever Python
started esphome and must not depend on the package being importable.

Subcommands:
    ar <ar-binary> <archive> <rspfile>   remove stale archive, then ``ar rc``
    copy <src> <dst>                     copy a file

The ar rspfile carries one object path per line (the generating rule must
use ``$in_newline``, never ``$in``).
"""

from pathlib import Path
import shutil
import subprocess
import sys


def _read_rspfile(rspfile: str) -> list[str]:
    r"""The object paths listed in ``rspfile``, unquoted.

    GNU ar treats backslashes in response files as escapes (corrupts
    Windows paths), so the caller expands the list into argv; strip the
    simple surrounding quote ninja adds to special paths, then undo
    ninja's POSIX escape for an embedded quote ('a'\\''b.o' -> a'b.o).
    """
    return [
        line[1:-1].replace("'\\''", "'")
        if len(line) >= 2 and line[0] == line[-1] and line[0] in "'\""
        else line
        for line in Path(rspfile).read_text(encoding="utf-8").splitlines()
        if line
    ]


def _run_ar(ar: str, archive: str, rspfile: str) -> int:
    # Remove first: ``ar rc`` replaces members but never drops ones whose
    # source was removed from the build, which would leak stale objects.
    Path(archive).unlink(missing_ok=True)
    objects = _read_rspfile(rspfile)
    if not objects:
        # An empty archive would "succeed" here and fail far away at link
        print(f"ar: no objects listed in {rspfile} for {archive}", file=sys.stderr)
        return 1
    # Batch by argv length: expanding the rspfile gives back the Windows
    # 32767-char command-line limit it existed to avoid. "rc" creates,
    # "q" appends the remainder.
    op = "rc"
    while objects:
        batch = [objects.pop(0)]
        batch_len = len(batch[0])
        while objects and batch_len + len(objects[0]) < 25000:
            batch_len += len(objects[0]) + 1
            batch.append(objects.pop(0))
        rc = subprocess.run(
            [ar, op, archive, *batch], check=False, close_fds=False
        ).returncode
        if rc != 0:
            # A failed batch must not leave a truncated archive behind
            Path(archive).unlink(missing_ok=True)
            return rc
        op = "q"
    return 0


def _run_copy(src: str, dst: str) -> int:
    shutil.copyfile(src, dst)
    return 0


def main() -> int:
    mode = sys.argv[1]
    if mode == "ar":
        return _run_ar(*sys.argv[2:5])
    if mode == "copy":
        return _run_copy(*sys.argv[2:4])
    print(f"unknown build_tool mode: {mode}", file=sys.stderr)
    return 1


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
