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

    def get_package(self, name: str) -> object | None:
        return None


class _BrokenPlatform(_FakePlatform):
    def get_package_version(self, name: str) -> str:
        raise RuntimeError("manifest parse error")


class _UnresolvedPlatform(_FakePlatform):
    """KeyError from a package that IS installed: unresolved identity."""

    def get_package_version(self, name: str) -> str:
        raise KeyError(name)

    def get_package(self, name: str) -> object:
        return object()


class _FakeSConsEnv(dict):
    """Just enough of a SCons construction environment for pch.py."""

    def __init__(
        self,
        proj_dir: Path,
        src_dir: Path,
        cxx: str,
        flags: list[str],
        platform_cls: type[_FakePlatform] = _FakePlatform,
    ):
        super().__init__(ENV={})
        self._subst = {
            "$PROJECT_DIR": str(proj_dir),
            "$PROJECT_SRC_DIR": str(src_dir),
            "$CXX": cxx,
        }
        self._flags = flags
        self._platform_cls = platform_cls
        self.prepended: list[str] = []

    def subst(self, expr: str) -> str:  # noqa: N802
        return self._subst[expr]

    def subst_list(self, expr: str) -> list[list[str]]:  # noqa: N802
        return [self._flags]

    def PioPlatform(self) -> _FakePlatform:  # noqa: N802
        return self._platform_cls()

    def Prepend(self, CXXFLAGS: list[str]) -> None:  # noqa: N802, N803
        self.prepended = CXXFLAGS


def _fake_cxx(
    tmp_path: Path,
    fail: bool = False,
    reject_pch: bool = False,
    probe_exit: int = 0,
) -> Path:
    """A compiler stand-in that records its argv and writes the -o target.

    With reject_pch it builds the .gch fine but, like GCC 10 on macOS arm64,
    warns on any consuming compile that the .gch cannot be loaded; probe_exit
    sets the exit code of non-header compiles (the load probe).
    """
    cxx = tmp_path / "fake-gxx"
    body = (
        'printf -- ---call---\\\\n >> "$0.argv"; printf \'%s\\n\' "$@" >> "$0.argv"\n'
    )
    if fail:
        body += "echo boom >&2\nexit 1\n"
    else:
        # Only the c++-header compile has a -o; the load probe has none
        body += 'out=""; prev=""\nfor a in "$@"; do [ "$prev" = "-o" ] && out="$a"; prev="$a"; done\n'
        body += '[ -n "$out" ] && echo gch > "$out"\n'
        if reject_pch:
            body += 'case " $* " in *c++-header*) ;; *) echo "warning: esphome_pch.h.gch: had text segment at different address" >&2;; esac\n'
        body += f'case " $* " in *c++-header*) exit 0;; *) exit {probe_exit};; esac\n'
    cxx.write_text("#!/bin/sh\n" + body)
    cxx.chmod(cxx.stat().st_mode | stat.S_IEXEC)
    return cxx


def _run_script(
    tmp_path: Path,
    flags: list[str] | None = None,
    fail: bool = False,
    reject_pch: bool = False,
    probe_exit: int = 0,
    env_vars: dict[str, str] | None = None,
    name: str = "dev",
    platform_cls: type[_FakePlatform] = _FakePlatform,
) -> _FakeSConsEnv:
    proj = tmp_path / name
    src = proj / "src"
    (src / "esphome" / "core").mkdir(parents=True, exist_ok=True)
    (src / "esphome" / "core" / "defines.h").write_text("#define USE_X\n")
    cxx = _fake_cxx(tmp_path, fail=fail, reject_pch=reject_pch, probe_exit=probe_exit)
    args = (proj, src, str(cxx), flags or ["-DX=1"], platform_cls)
    # Distinct objects: the script must scope ccache/flags to projenv only
    global_env = _FakeSConsEnv(*args)
    projenv = _FakeSConsEnv(*args)
    projenv.global_env = global_env
    source = _SCRIPT.read_text()
    with patch.dict(os.environ, env_vars or {}, clear=True):
        exec(  # noqa: S102
            compile(source, "pch.py", "exec"),
            {"Import": lambda *_names: None, "env": global_env, "projenv": projenv},
        )
    return projenv


def test_pch_script_builds_and_prepends_relative_include(tmp_path: Path) -> None:
    scons_env = _run_script(tmp_path)
    proj = tmp_path / "dev"
    assert (proj / "esphome_pch.h").read_text().endswith('"esphome/core/defines.h"\n')
    assert (proj / "esphome_pch.h.gch").is_file()
    assert len((proj / "esphome_pch.h.gch.sum").read_text().strip()) == 64
    # Relative include: an absolute path would poison ccache keys
    assert scons_env.prepended == ["-Winvalid-pch", "-include", "esphome_pch.h"]
    # ccache settings land on projenv's ENV only: framework/library TUs
    # compile under the global env and must keep strict hashing
    assert scons_env["ENV"]["CCACHE_SLOPPINESS"] == "pch_defines,time_macros"
    assert scons_env["ENV"]["CCACHE_PCH_EXTSUM"] == "true"
    assert scons_env.global_env["ENV"] == {}
    assert scons_env.global_env.prepended == []
    assert "CCACHE_SLOPPINESS" not in os.environ


def test_pch_script_preserves_spaced_flag_elements(tmp_path: Path) -> None:
    """One SCons element stays one compiler argv; -include pairs are
    stripped from the .gch compile."""
    spaced = tmp_path / "My Configs"
    spaced.mkdir()
    flags = ['-DUSB_PRODUCT=\\"Pico 2W\\"', "-I", str(spaced), "-include", "other.h"]
    _run_script(tmp_path, flags=flags)
    calls = (tmp_path / "fake-gxx.argv").read_text().split("---call---\n")
    gch_call = next(c for c in calls if "c++-header" in c).splitlines()
    assert '-DUSB_PRODUCT="Pico 2W"' in gch_call
    assert str(spaced) in gch_call
    assert "-include" not in gch_call
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


def test_pch_script_probe_rejection_falls_back(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A toolchain that cannot load its own .gch (GCC 10 on macOS arm64)
    must not leave consumers paying for a pch every compile rejects."""
    scons_env = _run_script(tmp_path, reject_pch=True)
    proj = tmp_path / "dev"
    assert not (proj / "esphome_pch.h.gch").exists()
    assert not (proj / "esphome_pch.h.gch.sum").exists()
    assert (proj / "esphome_pch.h.gch.failed").is_file()
    assert scons_env.prepended == []
    assert "toolchain cannot load the pch" in capsys.readouterr().out


def test_pch_script_probe_nonzero_exit_falls_back(tmp_path: Path) -> None:
    """A probe failure whose stderr never mentions .gch must still count."""
    scons_env = _run_script(tmp_path, probe_exit=1)
    proj = tmp_path / "dev"
    assert not (proj / "esphome_pch.h.gch").exists()
    assert (proj / "esphome_pch.h.gch.failed").is_file()
    assert scons_env.prepended == []


def test_pch_script_unresolved_package_version_skips_pch(tmp_path: Path) -> None:
    """A KeyError for an installed package is unresolved identity, not absence."""
    scons_env = _run_script(tmp_path, platform_cls=_UnresolvedPlatform)
    assert not (tmp_path / "dev" / "esphome_pch.h.gch").exists()
    assert scons_env.prepended == []


def test_pch_script_package_version_error_skips_pch(tmp_path: Path) -> None:
    """Without trustworthy package identity a stale .gch could survive an
    upgrade, so the script must not build one at all."""
    scons_env = _run_script(tmp_path, platform_cls=_BrokenPlatform)
    proj = tmp_path / "dev"
    assert not (proj / "esphome_pch.h.gch").exists()
    assert scons_env.prepended == []


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


@pytest.mark.skipif(os.geteuid() == 0, reason="root ignores file modes")
def test_pch_script_unreadable_local_header_warns_and_varies(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """An unreadable generated header still shifts the digest via mtime/size."""
    proj = tmp_path / "dev"
    override = proj / "lwip_override"
    override.mkdir(parents=True)
    secret = override / "lwipopts.h"
    secret.write_text("#define TCP_MSS 1460\n")
    secret.chmod(0)
    flags = ["-DX=1", "-I", str(override)]
    _run_script(tmp_path, flags=flags)
    first = (proj / "esphome_pch.h.gch.sum").read_text()
    assert "could not read" in capsys.readouterr().out
    os.utime(secret, (1, 1))
    (tmp_path / "fake-gxx.argv").unlink(missing_ok=True)
    _run_script(tmp_path, flags=flags)
    assert (proj / "esphome_pch.h.gch.sum").read_text() != first
