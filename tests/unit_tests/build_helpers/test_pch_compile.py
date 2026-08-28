"""Tests for the pch ninja edge shim (esphome.build_helpers.pch_compile)."""

from __future__ import annotations

import json
from pathlib import Path
import subprocess
from types import SimpleNamespace
from unittest.mock import patch

import pytest

from esphome.build_helpers import pch_compile

DIGEST = "a" * 64


def _fail_run(returncode: int = 1, stderr: str = "boom"):
    def run(cmd, **kwargs):
        return subprocess.CompletedProcess(cmd, returncode, "", stderr)

    return run


def _make_build(tmp_path: Path) -> SimpleNamespace:
    """A build dir with the header, a stub compile DB, and a digest sum."""
    build = tmp_path / "build"
    build.mkdir()
    src = tmp_path / "src" / "esphome"
    src.mkdir(parents=True)
    src_file = str(src / "a.cpp")
    (build / "compile_commands.json").write_text(
        json.dumps(
            [
                {
                    "directory": str(build),
                    "command": (
                        f'g++ -DX=1 -include esphome_pch.h -o a.cpp.obj -c "{src_file}"'
                    ),
                    "file": src_file,
                }
            ]
        )
    )
    header = build / "esphome_pch.h"
    header.write_text('#include "esphome/core/defines.h"\n', encoding="utf-8")
    gch = build / "esphome_pch.h.gch"
    (build / "esphome_pch.h.gch.sum").write_text(DIGEST + "\n", encoding="utf-8")
    return SimpleNamespace(
        build=build,
        header=header,
        gch=gch,
        sum=build / "esphome_pch.h.gch.sum",
        failed=build / "esphome_pch.h.gch.failed",
        depfile=build / "esphome_pch.h.gch.d",
        src_dir=tmp_path / "src",
    )


def _run_main(env: SimpleNamespace) -> int:
    argv = [
        "pch_compile",
        "--build-dir",
        str(env.build),
        "--src-dir",
        str(env.src_dir),
        "--header",
        str(env.header),
        "--gch",
        str(env.gch),
    ]
    with patch("sys.argv", argv):
        return pch_compile.main()


@pytest.fixture(autouse=True)
def _no_strict(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("ESPHOME_PCH_STRICT", raising=False)


def test_success_compiles_probes_and_clears_latch(tmp_path: Path) -> None:
    env = _make_build(tmp_path)
    env.failed.write_text("old\n", encoding="utf-8")
    seen: list[list[str]] = []

    def fake_run(cmd, **kwargs):
        seen.append(cmd)
        if "-fsyntax-only" in cmd:
            return subprocess.CompletedProcess(cmd, 0, "", "")
        env.gch.write_bytes(b"gch")
        mf = cmd[cmd.index("-MF") + 1]
        Path(mf).write_text(f"{env.gch}: {env.header}\n", encoding="utf-8")
        return subprocess.CompletedProcess(cmd, 0, "", "")

    with patch.object(pch_compile.subprocess, "run", side_effect=fake_run):
        assert _run_main(env) == 0
    compile_cmd = seen[0]
    assert compile_cmd[compile_cmd.index("-MT") + 1] == str(env.gch)
    assert env.gch.read_bytes() == b"gch"
    assert env.depfile.is_file()
    assert not env.failed.exists()
    assert env.sum.read_text().strip() == DIGEST


def test_deterministic_failure_latches_and_degrades(tmp_path: Path) -> None:
    env = _make_build(tmp_path)

    with patch.object(pch_compile.subprocess, "run", side_effect=_fail_run()):
        assert _run_main(env) == 0
    assert env.gch.read_bytes() == pch_compile.PLACEHOLDER
    assert env.sum.read_text().startswith("degraded:")
    assert env.failed.read_text().strip() == DIGEST
    assert env.depfile.is_file()


@pytest.mark.parametrize(
    ("stderr", "code"),
    [("fatal: No space left on device", 1), ("", -9)],
    ids=("enospc", "signal-kill"),
)
def test_transient_failures_do_not_latch(
    tmp_path: Path, stderr: str, code: int
) -> None:
    env = _make_build(tmp_path)

    with patch.object(
        pch_compile.subprocess, "run", side_effect=_fail_run(code, stderr)
    ):
        assert _run_main(env) == 0
    assert env.gch.read_bytes() == pch_compile.PLACEHOLDER
    assert not env.failed.exists()


def test_spawn_oserror_is_transient(tmp_path: Path) -> None:
    env = _make_build(tmp_path)
    with patch.object(
        pch_compile.subprocess, "run", side_effect=OSError("no such compiler")
    ):
        assert _run_main(env) == 0
    assert env.gch.read_bytes() == pch_compile.PLACEHOLDER
    assert not env.failed.exists()


def test_strict_failure_exits_nonzero(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("ESPHOME_PCH_STRICT", "1")
    env = _make_build(tmp_path)

    with patch.object(pch_compile.subprocess, "run", side_effect=_fail_run()):
        assert _run_main(env) != 0


def test_degraded_sum_skips_compile(tmp_path: Path) -> None:
    env = _make_build(tmp_path)
    env.sum.write_text("degraded:earlier failure latched\n", encoding="utf-8")
    with patch.object(pch_compile.subprocess, "run", side_effect=AssertionError):
        assert _run_main(env) == 0
    assert env.gch.read_bytes() == pch_compile.PLACEHOLDER
    assert env.depfile.is_file()


def test_empty_sum_skips_compile(tmp_path: Path) -> None:
    env = _make_build(tmp_path)
    env.sum.write_text("", encoding="utf-8")
    with patch.object(pch_compile.subprocess, "run", side_effect=AssertionError):
        assert _run_main(env) == 0
    assert env.gch.read_bytes() == pch_compile.PLACEHOLDER


def test_missing_compile_db_degrades(tmp_path: Path) -> None:
    env = _make_build(tmp_path)
    (env.build / "compile_commands.json").unlink()
    with patch.object(pch_compile.subprocess, "run", side_effect=AssertionError):
        assert _run_main(env) == 0
    assert env.gch.read_bytes() == pch_compile.PLACEHOLDER
    assert env.sum.read_text().startswith("degraded:")


def test_probe_rejection_latches(tmp_path: Path) -> None:
    """The gch builds but will not load: latch, blame the pch when the
    baseline compile passes."""
    env = _make_build(tmp_path)

    def run(cmd, **kwargs):
        if "-include" in cmd and "-fsyntax-only" in cmd:
            return subprocess.CompletedProcess(cmd, 1, "", "invalid pch")
        if "-fsyntax-only" in cmd:
            return subprocess.CompletedProcess(cmd, 0, "", "")
        env.gch.write_bytes(b"gch")
        return subprocess.CompletedProcess(cmd, 0, "", "")

    with patch.object(pch_compile.subprocess, "run", side_effect=run):
        assert _run_main(env) == 0
    assert env.gch.read_bytes() == pch_compile.PLACEHOLDER
    assert env.failed.read_text().strip() == DIGEST


def test_unexpected_command_shape_degrades(tmp_path: Path) -> None:
    env = _make_build(tmp_path)
    with (
        patch.object(
            pch_compile,
            "pch_compile_command",
            return_value=(["g++", "-weird"], env.build),
        ),
        patch.object(pch_compile.subprocess, "run", side_effect=AssertionError),
    ):
        assert _run_main(env) == 0
    assert env.gch.read_bytes() == pch_compile.PLACEHOLDER
