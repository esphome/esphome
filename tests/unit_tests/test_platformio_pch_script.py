"""Tests for esphome/platformio/pch.py.script against a fake SCons env."""

from __future__ import annotations

import os
from pathlib import Path
import stat
from unittest.mock import patch

import pytest

from esphome.platformio import toolchain

pytestmark = pytest.mark.skipif(
    os.name == "nt", reason="the fake compiler is a POSIX shell script"
)

_SCRIPT = Path(toolchain.__file__).parent / "pch.py.script"


class _FakePlatform:
    packages = {"framework-x": {}, "toolchain-y": {}}

    def get_package_version(self, name: str) -> str:
        if name == "toolchain-y":
            raise KeyError(name)
        return "1.2.3"


class _FakeSConsEnv(dict):
    """Just enough of a SCons construction environment for pch.py."""

    def __init__(self, proj_dir: Path, src_dir: Path, cxx: str, flags: list[str]):
        super().__init__(ENV={})
        self._subst = {
            "$PROJECT_DIR": str(proj_dir),
            "$PROJECT_SRC_DIR": str(src_dir),
            "$CXX": cxx,
        }
        self._flags = flags
        self.prepended: list[str] = []

    def subst(self, expr: str) -> str:  # noqa: N802
        return self._subst[expr]

    def subst_list(self, expr: str) -> list[list[str]]:  # noqa: N802
        return [self._flags]

    def PioPlatform(self) -> _FakePlatform:  # noqa: N802
        return _FakePlatform()

    def Prepend(self, CXXFLAGS: list[str]) -> None:  # noqa: N802, N803
        self.prepended = CXXFLAGS


def _fake_cxx(tmp_path: Path, fail: bool = False) -> Path:
    """A compiler stand-in that records its argv and writes the -o target."""
    cxx = tmp_path / "fake-gxx"
    body = 'printf \'%s\\n\' "$@" >> "$0.argv"\n'
    if fail:
        body += "echo boom >&2\nexit 1\n"
    else:
        body += 'out=""; prev=""\nfor a in "$@"; do [ "$prev" = "-o" ] && out="$a"; prev="$a"; done\necho gch > "$out"\n'
    cxx.write_text("#!/bin/sh\n" + body)
    cxx.chmod(cxx.stat().st_mode | stat.S_IEXEC)
    return cxx


def _run_script(
    tmp_path: Path,
    flags: list[str] | None = None,
    fail: bool = False,
    env_vars: dict[str, str] | None = None,
    name: str = "dev",
) -> _FakeSConsEnv:
    proj = tmp_path / name
    src = proj / "src"
    (src / "esphome" / "core").mkdir(parents=True, exist_ok=True)
    (src / "esphome" / "core" / "defines.h").write_text("#define USE_X\n")
    cxx = _fake_cxx(tmp_path, fail=fail)
    scons_env = _FakeSConsEnv(proj, src, str(cxx), flags or ["-DX=1"])
    source = _SCRIPT.read_text()
    with patch.dict(os.environ, env_vars or {}, clear=True):
        exec(  # noqa: S102
            compile(source, "pch.py", "exec"),
            {"Import": lambda *_names: None, "env": scons_env, "projenv": scons_env},
        )
    return scons_env


def test_pch_script_builds_and_prepends_relative_include(tmp_path: Path) -> None:
    scons_env = _run_script(tmp_path)
    proj = tmp_path / "dev"
    assert (proj / "esphome_pch.h").read_text().endswith('"esphome/core/defines.h"\n')
    assert (proj / "esphome_pch.h.gch").is_file()
    assert len((proj / "esphome_pch.h.gch.sum").read_text().strip()) == 64
    # Relative include: an absolute path would poison ccache keys
    assert scons_env.prepended == ["-include", "esphome_pch.h"]
    # ccache settings land on the SCons ENV only, never os.environ
    assert scons_env["ENV"]["CCACHE_SLOPPINESS"] == "pch_defines,time_macros"
    assert scons_env["ENV"]["CCACHE_PCH_EXTSUM"] == "true"
    assert "CCACHE_SLOPPINESS" not in os.environ


def test_pch_script_preserves_spaced_flag_elements(tmp_path: Path) -> None:
    """One SCons element stays one compiler argv; -include pairs are
    stripped from the .gch compile."""
    spaced = tmp_path / "My Configs"
    spaced.mkdir()
    flags = ['-DUSB_PRODUCT=\\"Pico 2W\\"', "-I", str(spaced), "-include", "other.h"]
    _run_script(tmp_path, flags=flags)
    argv = (tmp_path / "fake-gxx.argv").read_text().splitlines()
    assert '-DUSB_PRODUCT="Pico 2W"' in argv
    assert str(spaced) in argv
    assert "-include" not in argv
    # The stripped -include header is folded into the prefix header instead
    pch = (tmp_path / "dev" / "esphome_pch.h").read_text()
    assert pch.splitlines()[0] == '#include "other.h"'


def test_pch_script_sum_is_device_independent(tmp_path: Path) -> None:
    """Regression: identical configs in different dirs share cache keys."""
    sums = []
    for name in ("dev_a", "dev_b"):
        proj = tmp_path / name
        _run_script(
            tmp_path,
            flags=["-DX=1", "-I", str(proj / "include")],
            env_vars={"CCACHE_BASEDIR": str(proj)},
            name=name,
        )
        sums.append((proj / "esphome_pch.h.gch.sum").read_text())
        (tmp_path / "fake-gxx").unlink()
        (tmp_path / "fake-gxx.argv").unlink(missing_ok=True)
    assert sums[0] == sums[1]


def test_pch_script_failure_marker_suppresses_retry(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    scons_env = _run_script(tmp_path, fail=True)
    proj = tmp_path / "dev"
    assert (proj / "esphome_pch.h.gch.failed").is_file()
    assert not (proj / "esphome_pch.h.gch.sum").exists()
    assert scons_env.prepended == []
    # Second run: same checksum, no compile attempt, but says so
    attempts = (tmp_path / "fake-gxx.argv").read_text().count("c++-header")
    _run_script(tmp_path, fail=True)
    out = capsys.readouterr().out
    assert (tmp_path / "fake-gxx.argv").read_text().count("c++-header") == attempts
    assert "delete esphome_pch.h.gch.failed to retry" in out


def test_pch_script_rebuilds_when_header_missing(tmp_path: Path) -> None:
    _run_script(tmp_path)
    proj = tmp_path / "dev"
    (proj / "esphome_pch.h").unlink()
    _run_script(tmp_path)
    assert (proj / "esphome_pch.h").is_file()


def test_copy_pch_script(tmp_path: Path) -> None:
    from esphome.core import CORE

    CORE.build_path = tmp_path
    toolchain.copy_pch_script()
    assert (tmp_path / "pch.py").read_text() == _SCRIPT.read_text()


def test_pch_script_hashes_project_local_include_dirs(tmp_path: Path) -> None:
    """Generated headers in project-local -I dirs (e.g. rp2's lwip_override)
    must invalidate the checksum when they change."""
    proj = tmp_path / "dev"
    override = proj / "lwip_override"
    override.mkdir(parents=True)
    (override / "lwipopts.h").write_text("#define TCP_MSS 1460\n")
    flags = ["-DX=1", "-I", str(override)]
    _run_script(tmp_path, flags=flags)
    first = (proj / "esphome_pch.h.gch.sum").read_text()
    (override / "lwipopts.h").write_text("#define TCP_MSS 536\n")
    (tmp_path / "fake-gxx.argv").unlink(missing_ok=True)
    _run_script(tmp_path, flags=flags)
    assert (proj / "esphome_pch.h.gch.sum").read_text() != first
