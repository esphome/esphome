"""Tests for esphome/platformio/pch.py.script against a fake SCons env."""

from __future__ import annotations

from collections.abc import Callable
import os
from pathlib import Path
import stat
from types import SimpleNamespace
from unittest.mock import patch

import pytest

from esphome.build_helpers import pch
from esphome.platformio import toolchain

pytestmark = pytest.mark.skipif(
    os.name == "nt", reason="the fake compiler is a POSIX shell script"
)

_SCRIPT = Path(toolchain.__file__).parent / "pch.py.script"


class _FakePlatform:
    packages = {"framework-x": {}, "toolchain-y": {}}

    def get_package_version(self, name: str) -> str | None:
        # None for an optional package that is not installed
        return None if name == "toolchain-y" else "1.2.3"


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

    def Flatten(self, nodes: list) -> list:  # noqa: N802
        return nodes


def _fake_cxx(tmp_path: Path, fail: bool = False) -> Path:
    """A compiler stand-in that records its argv and writes the -o target."""
    cxx = tmp_path / "fake-gxx"
    body = (
        'printf -- ---call---\\\\n >> "$0.argv"; printf \'%s\\n\' "$@" >> "$0.argv"\n'
    )
    if fail:
        body += "echo boom >&2\nexit 1\n"
    else:
        body += 'out=""; prev=""\nfor a in "$@"; do [ "$prev" = "-o" ] && out="$a"; prev="$a"; done\n'
        body += '[ -n "$out" ] && echo gch > "$out"\n'
    cxx.write_text("#!/bin/sh\n" + body)
    cxx.chmod(cxx.stat().st_mode | stat.S_IEXEC)
    return cxx


def _run_script(
    tmp_path: Path,
    flags: list[str] | None = None,
    fail: bool = False,
    env_vars: dict[str, str] | None = None,
    name: str = "dev",
    platform_cls: type[_FakePlatform] = _FakePlatform,
    build_files: Callable[[tuple], list] | None = None,
) -> _FakeSConsEnv:
    proj = tmp_path / name
    src = proj / "src"
    (src / "esphome" / "core").mkdir(parents=True, exist_ok=True)
    (src / "esphome" / "core" / "defines.h").write_text("#define USE_X\n")
    (src / "esphome" / "core" / "pch_prefix.h").write_text("")
    cxx = _fake_cxx(tmp_path, fail=fail)
    args = (proj, src, str(cxx), flags or ["-DX=1"], platform_cls)
    # Distinct objects: the -include flags must land on projenv only
    global_env = _FakeSConsEnv(*args)
    projenv = _FakeSConsEnv(*args)
    projenv.global_env = global_env
    if build_files is not None:
        global_env["PIOBUILDFILES"] = build_files(args)
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
    assert (proj / "esphome_pch_src.h").read_text() == pch.pch_header_text(
        pch.PCH_DEFAULT_HEADERS
    )
    assert (proj / "esphome_pch.h").read_text() == pch.PCH_GUARD_TEXT
    assert (proj / "esphome_pch.h.gch").is_file()
    assert len((proj / "esphome_pch.h.gch.sum").read_text().strip()) == 64
    assert scons_env.prepended == pch.pch_consumer_flags()
    # The -include flags are scoped to projenv (src compiles)
    assert scons_env.global_env.prepended == []


def test_pch_script_names_match_the_python_side(tmp_path: Path) -> None:
    """The script cannot import esphome, so its copies are pinned."""
    namespace: dict[str, object] = {
        "Import": lambda *_names: None,
        "env": _FakeSConsEnv(tmp_path, tmp_path, "g++", []),
        "projenv": None,
    }
    exec(compile(_SCRIPT.read_text(), "pch.py", "exec"), namespace)  # noqa: S102
    assert namespace["_HEADER_NAME"] == pch.PCH_HEADER_NAME
    assert namespace["_SOURCE_NAME"] == pch.PCH_SOURCE_NAME
    assert namespace["_DEFAULT_HEADERS"] == pch.PCH_DEFAULT_HEADERS
    assert namespace["_CONSUMER_FLAGS"] == pch.pch_consumer_flags()
    assert namespace["_GUARD_TEXT"] == pch.PCH_GUARD_TEXT
    assert namespace["_INCLUDE_RE"].pattern == pch._INCLUDE_RE.pattern


def test_pch_script_compile_failure_stops_the_build(tmp_path: Path) -> None:
    """The pch holds only ESPHome's own headers: a failure is a defect."""
    with pytest.raises(RuntimeError, match="could not compile") as raised:
        _run_script(tmp_path, fail=True)
    assert "boom" in str(raised.value)
    assert not (tmp_path / "dev" / "esphome_pch.h.gch.sum").exists()


def test_pch_script_preserves_spaced_flag_elements(tmp_path: Path) -> None:
    """One SCons element stays one compiler argv; -include pairs are
    stripped from the .gch compile."""
    spaced = tmp_path / "My Configs"
    spaced.mkdir()
    (tmp_path / "dev" / "src").mkdir(parents=True, exist_ok=True)
    (tmp_path / "dev" / "src" / "other.h").write_text("")
    flags = ['-DUSB_PRODUCT=\\"Pico 2W\\"', "-I", str(spaced), "-include", "other.h"]
    _run_script(tmp_path, flags=flags)
    calls = (tmp_path / "fake-gxx.argv").read_text().split("---call---\n")
    gch_call = next(c for c in calls if "c++-header" in c).splitlines()
    assert '-DUSB_PRODUCT="Pico 2W"' in gch_call
    assert str(spaced) in gch_call
    assert "-include" not in gch_call
    # The stripped src-resolvable -include is folded into the prefix header
    pch = (tmp_path / "dev" / "esphome_pch_src.h").read_text()
    assert pch.splitlines()[0] == '#include "other.h"'


def test_pch_script_folds_joined_force_include_spelling(tmp_path: Path) -> None:
    """-includefoo.h folds like the separated form, matching the native path."""
    (tmp_path / "dev" / "src").mkdir(parents=True, exist_ok=True)
    (tmp_path / "dev" / "src" / "other.h").write_text("")
    _run_script(tmp_path, flags=["-DX=1", "-includeother.h"])
    pch = (tmp_path / "dev" / "esphome_pch_src.h").read_text()
    assert pch.splitlines()[0] == '#include "other.h"'


def test_pch_script_leaves_absolute_force_includes_unfolded(
    tmp_path: Path,
) -> None:
    """An absolute -include resolves through src_dir / name; it must still
    stay consumer-only or the host path enters the .sum."""
    outside = tmp_path / "outside.h"
    outside.write_text("")
    _run_script(tmp_path, flags=["-DX=1", "-include", str(outside)])
    pch = (tmp_path / "dev" / "esphome_pch_src.h").read_text()
    assert "outside.h" not in pch


def test_pch_script_leaves_non_src_force_includes_unfolded(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A user -include outside src/ stays on the consumers only."""
    _run_script(tmp_path, flags=["-DX=1", "-include", "user_extra.h"])
    pch = (tmp_path / "dev" / "esphome_pch_src.h").read_text()
    assert "user_extra.h" not in pch
    assert "not precompiling non-src force-includes" in capsys.readouterr().out


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


def test_pch_script_uses_the_envs_of_existing_src_objects(tmp_path: Path) -> None:
    """The ESP-IDF builder creates the src objects from its own environments
    before this script runs; flags on projenv would never reach them."""
    made: list[_FakeSConsEnv] = []

    def build_files(args: tuple) -> list:
        src = args[1]
        made.extend(_FakeSConsEnv(*args) for _ in range(3))
        return [
            SimpleNamespace(env=made[0], sources=[src / "main.cpp"]),
            SimpleNamespace(env=made[0], sources=[src / "esphome" / "a.cpp"]),
            # C and framework objects keep their environments untouched
            SimpleNamespace(env=made[1], sources=[src / "esphome" / "b.c"]),
            SimpleNamespace(env=made[2], sources=[src.parent / "lib" / "c.cpp"]),
        ]

    projenv = _run_script(tmp_path, build_files=build_files)
    assert made[0].prepended == pch.pch_consumer_flags()
    assert made[1].prepended == made[2].prepended == projenv.prepended == []


def test_copy_pch_script(tmp_path: Path) -> None:
    from esphome.core import CORE

    CORE.build_path = tmp_path
    toolchain.copy_pch_script()
    assert (tmp_path / "pch.py").read_text() == _SCRIPT.read_text()


def test_pch_script_nobuild_without_projenv_is_noop(tmp_path: Path) -> None:
    """-t nobuild never exports projenv; the script must not abort."""
    proj = tmp_path / "dev"
    (proj / "src").mkdir(parents=True)

    def strict_import(*names: str) -> None:
        if "projenv" in names:
            raise RuntimeError("Import of non-existent variable 'projenv'")

    env = _FakeSConsEnv(proj, proj / "src", "g++", ["-DX=1"])
    exec(  # noqa: S102
        compile(_SCRIPT.read_text(), "pch.py", "exec"),
        {"Import": strict_import, "env": env},
    )
    assert not (proj / "esphome_pch.h").exists()


def test_pch_script_ignores_library_trees_and_non_headers(tmp_path: Path) -> None:
    """.piolibdeps and non-header files must not enter the digest (or be
    read at all); package versions already cover library identity."""
    proj = tmp_path / "dev"
    libdeps = proj / ".piolibdeps" / "lib" / "src"
    libdeps.mkdir(parents=True)
    (libdeps / "lib.h").write_text("#define A 1\n")
    override = proj / "lwip_override"
    override.mkdir(parents=True)
    (override / "lwipopts.h").write_text("#define TCP_MSS 1460\n")
    (override / "notes.txt").write_text("v1\n")
    flags = ["-DX=1", "-I", str(libdeps), "-I", str(override)]
    _run_script(tmp_path, flags=flags)
    first = (proj / "esphome_pch.h.gch.sum").read_text()
    (libdeps / "lib.h").write_text("#define A 2\n")
    (override / "notes.txt").write_text("v2\n")
    (tmp_path / "fake-gxx.argv").unlink(missing_ok=True)
    _run_script(tmp_path, flags=flags)
    assert (proj / "esphome_pch.h.gch.sum").read_text() == first


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
