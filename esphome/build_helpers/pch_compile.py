"""Ninja edge that compiles the precompiled header inside the build graph.

Invoked as ``python -m esphome.build_helpers.pch_compile`` by the custom
command the espidf backend bakes into the src component CMakeLists. The
pre-ninja side (``prepare_pch_sidecars``) owns the identity digest and
writes ``.sum``; this edge reads it, compiles and probes the .gch, and on
failure degrades exactly like the pre-build flow: placeholder .gch (its
``-Winvalid-pch`` warning makes consumers parse the header textually),
``degraded:`` ``.sum`` so ccache never keys on a broken pch, and the
``.failed`` latch for deterministic errors. Exit 0 keeps the build going;
``ESPHOME_PCH_STRICT`` turns every degrade into a nonzero exit.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys

from esphome.build_helpers.pch import (
    is_transient_error,
    pch_compile_command,
    pch_probe_args,
    pch_probe_tail,
    pch_strict,
    read_stamp,
)

# Deliberately not a valid GCH: consumers warn via -Winvalid-pch and fall
# back to the textual include, matching the pre-build degrade behavior
PLACEHOLDER = b"ESPHome degraded precompiled header placeholder\n"


def _log(msg: str) -> None:
    print(f"esphome pch: {msg}", file=sys.stderr)


def _write_depfile(depfile: Path, gch: Path, header: Path) -> None:
    """Ninja errors on a declared-but-missing depfile; keep a minimal one
    on every non-compile path. The target is the ninja-relative output."""
    depfile.write_text(f"{gch.name}: {header}\n", encoding="utf-8")


def _degrade(
    args: argparse.Namespace, reason: str, latch_digest: str | None = None
) -> int:
    gch = Path(args.gch)
    header = Path(args.header)
    _log(f"compiling without the precompiled header: {reason}")
    if pch_strict():
        _log("ESPHOME_PCH_STRICT is set; failing the build")
        return 1
    sum_path = Path(f"{gch}.sum")
    if not read_stamp(sum_path).startswith("degraded:"):
        sum_path.write_text(f"degraded:{reason[:120]}\n", encoding="utf-8")
    if latch_digest is not None:
        Path(f"{gch}.failed").write_text(latch_digest + "\n", encoding="utf-8")
    gch.write_bytes(PLACEHOLDER)
    _write_depfile(Path(f"{gch}.d"), gch, header)
    return 0


def _run(
    cmd: list[str], cwd: Path, stdin: str | None = None
) -> subprocess.CompletedProcess | None:
    """One tool step; None for environmental failures (never latched)."""
    try:
        return subprocess.run(
            cmd,
            cwd=cwd,
            env={**os.environ, "LC_ALL": "C"},
            input=stdin,
            capture_output=True,
            text=True,
            check=False,
            timeout=300,
        )
    except (OSError, subprocess.SubprocessError) as err:
        _log(f"tool did not run: {err}")
        return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--src-dir", required=True)
    parser.add_argument("--header", required=True)
    parser.add_argument("--gch", required=True)
    args = parser.parse_args()

    build_dir = Path(args.build_dir)
    header = Path(args.header)
    gch = Path(args.gch)
    digest = read_stamp(Path(f"{gch}.sum"))
    if not digest or digest.startswith("degraded:"):
        # The pre-ninja side already decided to degrade this build
        return _degrade(args, "sidecar marks the pch degraded")

    cmd_and_dir = pch_compile_command(
        build_dir, header, gch, src_dir=Path(args.src_dir)
    )
    if cmd_and_dir is None:
        return _degrade(args, "no usable compile command")
    cmd, cmd_dir = cmd_and_dir
    if cmd[-6:-4] != ["-x", "c++-header"]:
        # The probe slice below depends on pch_compile_command's fixed tail
        return _degrade(args, "unexpected pch command shape")
    # The graph edge owns dependency tracking; the target name must match
    # ninja's spelling of the output (build-dir relative)
    depfile = Path(f"{gch}.d")
    compile_cmd = [*cmd, "-MD", "-MF", str(depfile), "-MT", gch.name]

    result = _run(compile_cmd, cmd_dir)
    if result is None:
        return _degrade(args, "compiler did not run")
    if result.returncode < 0:
        return _degrade(args, f"compiler killed by signal {-result.returncode}")
    if result.returncode != 0 or not gch.is_file():
        error = result.stderr.strip() or f"exit code {result.returncode}"
        _log(error[:1000])
        latch = None if is_transient_error(error) else digest
        return _degrade(args, "compile failed", latch_digest=latch)

    base = cmd[:-6]  # strip the fixed "-x c++-header -c <hdr> -o <gch>" tail
    probe = _run([*base, *pch_probe_args(str(header))], cmd_dir, stdin="")
    if probe is None:
        return _degrade(args, "probe did not run")
    if probe.returncode != 0:
        # Only blame the pch when the same compile passes without it
        baseline = _run([*base, *pch_probe_tail()], cmd_dir, stdin="")
        error = probe.stderr.strip() or f"exit code {probe.returncode}"
        _log(error[:1000])
        latch = None if is_transient_error(error) else digest
        if baseline is not None and baseline.returncode != 0:
            latch = None if is_transient_error(baseline.stderr) else digest
        return _degrade(args, "toolchain cannot load the pch", latch_digest=latch)

    Path(f"{gch}.failed").unlink(missing_ok=True)
    if not depfile.is_file():
        _write_depfile(depfile, gch, header)
    return 0


if __name__ == "__main__":
    sys.exit(main())
