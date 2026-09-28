"""Tests for esphome.build_helpers.pch."""

from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess
from unittest.mock import patch

import pytest

from esphome.build_helpers import pch
from esphome.const import KEY_CORE, KEY_TARGET_PLATFORM
from esphome.core import CORE, EsphomeError


def _write(src_dir: Path, name: str, content: str) -> None:
    path = src_dir / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        (None, True),
        ("1", True),
        ("0", False),
        ("false", False),
        ("", False),
    ],
)
def test_pch_enabled(value: str | None, expected: bool) -> None:
    env = {} if value is None else {"ESPHOME_PCH_ENABLE": value}
    with patch.dict(os.environ, env, clear=True):
        assert pch.pch_enabled() is expected


def test_ccache_pch_env_enabled() -> None:
    with patch.dict(os.environ, {}, clear=True):
        env = pch.ccache_pch_env()
    assert env == {
        "CCACHE_SLOPPINESS": "pch_defines,time_macros",
        "CCACHE_PCH_EXTSUM": "true",
    }


def test_ccache_pch_env_keeps_user_values() -> None:
    """A user sloppiness list without the pch entries would stop ccache
    from caching every compile that loads the .gch."""
    user = {"CCACHE_SLOPPINESS": "locale, time_macros", "CCACHE_PCH_EXTSUM": "false"}
    with patch.dict(os.environ, user, clear=True):
        assert pch.ccache_pch_env() == {
            "CCACHE_SLOPPINESS": "locale,time_macros,pch_defines"
        }


def test_ccache_pch_env_disabled() -> None:
    with patch.dict(os.environ, {"ESPHOME_PCH_ENABLE": "0"}, clear=True):
        assert pch.ccache_pch_env() == {}


def test_pch_header_text_preserves_order() -> None:
    text = pch.pch_header_text(["b.h", "a.h"])
    assert text == '#include "b.h"\n#include "a.h"\n'


def test_include_closure_resolves_relative_and_root(tmp_path: Path) -> None:
    """Sibling includes resolve against the includer's directory first,
    full paths against the src root; unresolvable names end the walk."""
    _write(tmp_path, "esphome/components/x/a.h", '#include "b.h"\n')
    _write(
        tmp_path,
        "esphome/components/x/b.h",
        '#include "esphome/core/deep.h"\n#include <system.h>\n#include "missing.h"\n',
    )
    _write(tmp_path, "esphome/core/deep.h", "")
    closure = pch._include_closure(tmp_path, ["esphome/components/x/a.h"])
    assert sorted(closure) == [
        "esphome/components/x/a.h",
        "esphome/components/x/b.h",
        "esphome/core/deep.h",
    ]


def test_include_closure_handles_cycles(tmp_path: Path) -> None:
    _write(tmp_path, "a.h", '#include "b.h"\n')
    _write(tmp_path, "b.h", '#include "a.h"\n')
    assert sorted(pch._include_closure(tmp_path, ["a.h"])) == ["a.h", "b.h"]


def test_include_closure_blocks_parent_escape(tmp_path: Path) -> None:
    _write(tmp_path / "src", "a.h", '#include "../outside.h"\n')
    (tmp_path / "outside.h").write_text("")
    assert sorted(pch._include_closure(tmp_path / "src", ["a.h"])) == ["a.h"]


def test_pch_checksum_tracks_closure_content(tmp_path: Path) -> None:
    """A transitive header edit or an extra-identity change must change the
    digest; unrelated files must not."""
    _write(tmp_path, "root.h", '#include "nested.h"\n')
    _write(tmp_path, "nested.h", "int a;\n")
    _write(tmp_path, "unrelated.h", "int u;\n")
    base = pch.pch_checksum(tmp_path, ["root.h"], ["id"])
    assert base == pch.pch_checksum(tmp_path, ["root.h"], ["id"])
    assert base != pch.pch_checksum(tmp_path, ["root.h"], ["other-id"])
    _write(tmp_path, "unrelated.h", "int changed;\n")
    assert base == pch.pch_checksum(tmp_path, ["root.h"], ["id"])
    _write(tmp_path, "nested.h", "int b;\n")
    assert base != pch.pch_checksum(tmp_path, ["root.h"], ["id"])


@pytest.mark.parametrize(
    "platform", ["bk72xx", "esp32", "esp8266", "ln882x", "rp2", "rtl87xx"]
)
def test_pch_script_enabled(platform: str, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(pch.sys, "platform", "linux")
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: platform}
    assert pch.pch_script_enabled()
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    assert not pch.pch_script_enabled()


@pytest.mark.parametrize("host", ["darwin", "win32"])
@pytest.mark.parametrize("platform", sorted(pch.PCH_SCRIPT_LINUX_ONLY_PLATFORMS))
def test_pch_script_linux_only_platform(
    platform: str, host: str, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setattr(pch.sys, "platform", host)
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: platform}
    assert not pch.pch_script_enabled()


@pytest.mark.parametrize("platform", sorted(pch.PCH_SCRIPT_EXCLUDED_PLATFORMS))
def test_pch_script_excluded_platform(platform: str) -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: platform}
    assert not pch.pch_script_enabled()


def test_include_closure_walks_angle_includes_under_src(tmp_path: Path) -> None:
    """An angle include resolving under src/ must enter the digest; one
    that does not simply ends the walk."""
    _write(tmp_path, "a.h", "#include <local.h>\n#include <Arduino.h>\n")
    (tmp_path / "local.h").write_text("")
    closure = pch._include_closure(tmp_path, ["a.h"])
    assert set(closure) == {"a.h", "local.h"}


def test_pch_cmake_consumer_substitutes_target_and_sources(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    block = pch.pch_cmake_consumer("app", "${APP_SOURCES}")
    assert "target_compile_options(app PRIVATE" in block
    assert '"$<$<COMPILE_LANGUAGE:CXX>:-Winvalid-pch>"' in block
    assert '"$<$<COMPILE_LANGUAGE:CXX>:-Werror=invalid-pch>"' in block
    assert '"$<$<COMPILE_LANGUAGE:CXX>:esphome_pch.h>"' in block
    # C sources do not load the pch, so they must not rebuild with it
    assert "set(esphome_pch_sources ${APP_SOURCES})" in block
    assert 'list(FILTER esphome_pch_sources EXCLUDE REGEX "[.][cSs]$")' in block
    assert "set_source_files_properties(${esphome_pch_sources} PROPERTIES" in block
    assert 'OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/esphome_pch.h"' in block


def test_pch_cmake_consumer_empty_when_disabled(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    assert pch.pch_cmake_consumer("app", "${APP_SOURCES}") == ""


def _make_pch_device(tmp_path: Path, name: str) -> Path:
    """A device dir with the core headers, a configuration file and a
    one-entry compile database."""
    dev = tmp_path / name
    for header in pch.PCH_DEFAULT_HEADERS:
        path = dev / "src" / header
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "esphome/core/defines.h"\n')
    (dev / "src" / "esphome" / "core" / "defines.h").write_text(
        '#include "esphome/core/macros.h"\n'
    )
    (dev / "src" / "esphome" / "core" / "macros.h").write_text("#define M 1\n")
    (dev / "config.h").write_text("CONFIG_X=y\n")
    build = dev / "build"
    build.mkdir()
    src_file = str(dev / "src" / "esphome" / "a.cpp")
    _write_db(
        build,
        f'g++ -DX=1 -include esphome_pch.h -o a.cpp.obj -c "{src_file}"',
        src_file,
    )
    return dev


def _write_db(build: Path, command: str, file: str) -> None:
    (build / "compile_commands.json").write_text(
        json.dumps([{"directory": str(build), "command": command, "file": file}])
    )


def _prepare(dev: Path, returncode: int = 0) -> str:
    """Run prepare_pch with a stub compiler; return the .sum text."""
    CORE.build_path = dev

    def compile_(cmd: list[str], **kwargs: object) -> subprocess.CompletedProcess:
        # The compile must target the include list, not the guard header
        assert cmd[-5:-2] == [
            "c++-header",
            "-c",
            str(dev / "build" / "esphome_pch_src.h"),
        ]
        if returncode == 0:
            (dev / "build" / "esphome_pch.h.gch").write_bytes(b"gch")
        return subprocess.CompletedProcess(cmd, returncode, "", "boom")

    with patch("esphome.build_helpers.pch.subprocess.run", side_effect=compile_):
        pch.prepare_pch(dev / "build", dev / "config.h", ("1.2.3",))
    return (dev / "build" / "esphome_pch.h.gch.sum").read_text()


def test_prepare_pch_writes_header_and_sum(tmp_path: Path) -> None:
    dev = _make_pch_device(tmp_path, "dev_a")
    assert len(_prepare(dev).strip()) == 64
    # A compiler that skips the .gch reads this header: it must be an error
    assert "#error" in (dev / "build" / "esphome_pch.h").read_text()
    # Unchanged inputs: the second call must not recompile
    with patch("esphome.build_helpers.pch.subprocess.run", side_effect=AssertionError):
        pch.prepare_pch(dev / "build", dev / "config.h", ("1.2.3",))


@pytest.mark.parametrize("user_basedir", [False, True])
def test_prepare_pch_sum_has_no_device_path(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, user_basedir: bool
) -> None:
    """The per-device build path would stop ccache sharing between devices."""
    if user_basedir:
        # A parent of the build path must not shadow it
        monkeypatch.setenv("CCACHE_BASEDIR", str(tmp_path))
    sums = [_prepare(_make_pch_device(tmp_path, name)) for name in ("dev_a", "dev_b")]
    assert sums[0] == sums[1]


def test_pch_compile_command_variants(tmp_path: Path) -> None:
    """Missing DB, no matching entry, and launcher-prefixed commands."""
    build = tmp_path / "build"
    build.mkdir()
    header = build / "esphome_pch.h"
    gch = build / "esphome_pch.h.gch"
    with pytest.raises(FileNotFoundError):
        pch.pch_compile_command(build, header, gch)

    _write_db(build, "gcc -c other.c", "other.c")
    with pytest.raises(EsphomeError, match="has no ESPHome"):
        pch.pch_compile_command(build, header, gch)

    src_file = str(tmp_path / "src" / "esphome" / "a.cpp")
    _write_db(
        build,
        "/usr/bin/ccache g++ -DX=1 -include user.h -include esphome_pch.h -MMD "
        f"-MT a.cpp.obj -MF a.cpp.obj.d -o a.cpp.obj -c {src_file}",
        src_file,
    )
    # Launcher, the pch -include, -o/-c and depfile flags removed
    cmd, cmd_dir = pch.pch_compile_command(build, header, gch)
    assert cmd == [
        "g++",
        "-DX=1",
        "-include",
        "user.h",
        "-x",
        "c++-header",
        "-c",
        str(header),
        "-o",
        str(gch),
    ]
    # The compile must run where the flags were resolved
    assert cmd_dir == build


def test_prepare_pch_compile_failure_stops_the_build(tmp_path: Path) -> None:
    dev = _make_pch_device(tmp_path, "dev_f")
    # An earlier .sum must not outlive the .gch it was written for
    (dev / "build" / "esphome_pch.h.gch.sum").write_text("old\n")
    with pytest.raises(EsphomeError, match="ESPHOME_PCH_ENABLE=0.*: boom"):
        _prepare(dev, returncode=1)
    assert not (dev / "build" / "esphome_pch.h.gch.sum").exists()


def test_prepare_pch_missing_configuration_stops_the_build(tmp_path: Path) -> None:
    dev = _make_pch_device(tmp_path, "dev_m")
    (dev / "config.h").unlink()
    with pytest.raises(EsphomeError, match="prepare the precompiled header"):
        _prepare(dev)


def test_prepare_pch_broken_compile_database_stops_the_build(tmp_path: Path) -> None:
    dev = _make_pch_device(tmp_path, "dev_j")
    (dev / "build" / "compile_commands.json").write_text("[{")
    with pytest.raises(EsphomeError, match="ESPHOME_PCH_ENABLE=0"):
        _prepare(dev)


def test_prepare_pch_bumps_header_for_object_depends(tmp_path: Path) -> None:
    """Consumers depend on the header, so a rebuilt .gch must bump it."""
    dev = _make_pch_device(tmp_path, "dev_t")
    pch.write_pch_headers(dev / "build", pch.PCH_DEFAULT_HEADERS)
    header = dev / "build" / "esphome_pch.h"
    os.utime(header, (0, 0))
    _prepare(dev)
    assert header.stat().st_mtime > 0


def test_prepare_pch_command_change_invalidates_sum(tmp_path: Path) -> None:
    """A flag-only change in the compile DB must rebuild the .gch."""
    dev = _make_pch_device(tmp_path, "dev_c")
    first = _prepare(dev)
    db = dev / "build" / "compile_commands.json"
    db.write_text(db.read_text().replace("-DX=1", "-DX=2"))
    assert _prepare(dev) != first
