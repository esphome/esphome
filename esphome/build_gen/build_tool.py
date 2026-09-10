"""Tiny cross-platform build steps invoked from the generated ninja file.

Plain script (not ``python -m``): it runs from ninja with whatever Python
started esphome and must not depend on the package being importable.

Subcommands:
    ar <ar-binary> <archive> <rspfile>   remove stale archive, then ``ar rcs``
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
    # Remove first: ``ar rcs`` replaces members but never drops ones whose
    # source was removed from the build, which would leak stale objects.
    Path(archive).unlink(missing_ok=True)
    objects = _read_rspfile(rspfile)
    if not objects:
        # An empty archive would "succeed" here and fail far away at link
        print(f"ar: no objects listed in {rspfile} for {archive}", file=sys.stderr)
        return 1
    # Batch by argv length: expanding the rspfile gives back the Windows
    # 32767-char command-line limit it existed to avoid. "rcs" creates,
    # "qs" appends; the s keeps the symbol index explicit on every ar.
    op = "rcs"
    ok = False
    try:
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
                return rc
            op = "qs"
        ok = True
        return 0
    finally:
        if not ok:
            # Any failure (bad exit, missing ar binary, interrupt) must not
            # leave a truncated archive behind
            Path(archive).unlink(missing_ok=True)


def _run_copy(src: str, dst: str) -> int:
    try:
        shutil.copyfile(src, dst)
    except OSError as err:
        # Never leave a partially written output (e.g. a firmware image);
        # SameFileError means dst IS src, where unlinking destroys the input
        if not isinstance(err, shutil.SameFileError):
            Path(dst).unlink(missing_ok=True)
        print(f"copy: {src} -> {dst} failed: {err}", file=sys.stderr)
        return 1
    return 0


# mode -> (handler, expected operand count); surplus argv means a
# mis-specified ninja rule and must error, not silently drop operands
_MODES = {"ar": (_run_ar, 3), "copy": (_run_copy, 2)}


def main() -> int:
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    if entry := _MODES.get(mode):
        handler, argc = entry
        args = sys.argv[2:]
        if len(args) != argc:
            print(
                f"build_tool {mode}: expected {argc} arguments, got {len(args)}",
                file=sys.stderr,
            )
            return 1
        return handler(*args)
    print(f"unknown build_tool mode: {mode}", file=sys.stderr)
    return 1


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
