"""Ninja edge that compiles the precompiled header inside the build graph.

Run as ``python -m esphome.build_helpers.pch_compile`` by the command
_pch_edge_cmake bakes into the src CMakeLists; ``-m`` (not a plain script)
so it reuses the pch helpers instead of carrying copies. Degrades exactly
like the pre-build flow; exit 0 keeps the build going and
``ESPHOME_PCH_STRICT`` turns every degrade into a nonzero exit.
"""

from __future__ import annotations

import argparse
import logging
from pathlib import Path
import subprocess
import sys

from esphome.build_helpers.pch import (
    degraded_sum_text,
    is_degraded_sum,
    is_transient_error,
    pch_compile_command,
    pch_probe_args,
    pch_probe_base,
    pch_probe_tail,
    pch_run_tool,
    pch_strict,
    read_stamp,
)

_LOGGER = logging.getLogger(__name__)

# Deliberately not a valid GCH: consumers warn via -Winvalid-pch and fall
# back to the textual include, matching the pre-build degrade behavior
PLACEHOLDER = b"ESPHome degraded precompiled header placeholder\n"


def _write_depfile(depfile: Path, gch: Path, header: Path) -> None:
    """Ninja errors on a declared-but-missing depfile; keep a minimal one
    on every non-compile path. Absolute target, as the compile's -MT."""
    depfile.write_text(f"{gch}: {header}\n", encoding="utf-8")


def _degrade(
    gch: Path, header: Path, reason: str, latch_digest: str | None = None
) -> int:
    _LOGGER.warning("compiling without the precompiled header: %s", reason)
    if pch_strict():
        _LOGGER.warning("ESPHOME_PCH_STRICT is set; failing the build")
        return 1
    sum_path = Path(f"{gch}.sum")
    # Never clobber the pre-ninja side's own degrade reason
    if not is_degraded_sum(read_stamp(sum_path)):
        sum_path.write_text(degraded_sum_text(reason), encoding="utf-8")
    if latch_digest is not None:
        Path(f"{gch}.failed").write_text(latch_digest + "\n", encoding="utf-8")
    gch.write_bytes(PLACEHOLDER)
    _write_depfile(Path(f"{gch}.d"), gch, header)
    return 0


def _run(
    cmd: list[str], cwd: Path, stdin: str | None = None
) -> subprocess.CompletedProcess | None:
    """None for environmental spawn failures (never latched)."""
    try:
        return pch_run_tool(cmd, cwd, stdin)
    except (OSError, subprocess.SubprocessError) as err:
        _LOGGER.warning("tool did not run: %s", err)
        return None


def main() -> int:
    logging.basicConfig(format="esphome pch: %(message)s")
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
    if is_degraded_sum(digest):
        # The pre-ninja side already decided to degrade this build
        return _degrade(gch, header, "sidecar marks the pch degraded")
    if not digest:
        return _degrade(gch, header, "no digest sidecar")

    cmd_and_dir = pch_compile_command(build_dir, header, gch, Path(args.src_dir))
    if cmd_and_dir is None:
        return _degrade(gch, header, "no usable compile command")
    cmd, cmd_dir = cmd_and_dir
    base = pch_probe_base(cmd)
    if base is None:
        return _degrade(gch, header, "unexpected pch command shape")
    # The graph edge owns dependency tracking; the -MT target must be the
    # absolute output path so cmake's depfile transform (CMP0116) maps it
    # to ninja's spelling — a relative name resolves against the component
    # binary dir and never matches, leaving the edge forever dirty
    depfile = Path(f"{gch}.d")
    compile_cmd = [*cmd, "-MD", "-MF", str(depfile), "-MT", str(gch)]

    result = _run(compile_cmd, cmd_dir)
    if result is None:
        return _degrade(gch, header, "compiler did not run")
    if result.returncode < 0:
        return _degrade(gch, header, f"compiler killed by signal {-result.returncode}")
    if result.returncode != 0 or not gch.is_file():
        error = result.stderr.strip() or f"exit code {result.returncode}"
        _LOGGER.warning("%s", error[:1000])
        latch = None if is_transient_error(error) else digest
        return _degrade(gch, header, "compile failed", latch_digest=latch)

    probe = _run([*base, *pch_probe_args(str(header))], cmd_dir, stdin="")
    if probe is None:
        return _degrade(gch, header, "probe did not run")
    if probe.returncode != 0:
        # Blame the pch only when the same compile passes without it
        baseline = _run([*base, *pch_probe_tail()], cmd_dir, stdin="")
        if baseline is not None and baseline.returncode != 0:
            error = baseline.stderr.strip() or f"exit code {baseline.returncode}"
            reason = "probe cannot run at all"
        else:
            error = probe.stderr.strip() or f"exit code {probe.returncode}"
            reason = "toolchain cannot load the pch"
        _LOGGER.warning("%s", error[:1000])
        latch = None if is_transient_error(error) else digest
        return _degrade(gch, header, reason, latch_digest=latch)

    Path(f"{gch}.failed").unlink(missing_ok=True)
    if not depfile.is_file():
        # Only if the compiler ignored -MD; ninja hard-errors without one
        _write_depfile(depfile, gch, header)
    return 0


if __name__ == "__main__":
    sys.exit(main())
