"""Tests for esphome.build_helpers.pch."""

from __future__ import annotations

import logging
import os
from pathlib import Path
import sys
from unittest.mock import patch

import pytest

from esphome.build_helpers import pch
from esphome.const import KEY_CORE, KEY_TARGET_PLATFORM
from esphome.core import CORE


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


@pytest.mark.parametrize(
    ("version", "expected"),
    [
        ((), False),
        ((10, 3), False),
        ((12, 2, 1), False),
        ((14, 2, 0), False),
        ((14, 3), False),
        ((14, 4), True),
        ((14,), False),
        ((15, 2, 0), False),
        ((15, 3), True),
        ((16, 0), True),
        ((17, 1), True),
    ],
)
def test_gcc_relocates_pch_on_windows(version: tuple[int, ...], expected: bool) -> None:
    assert pch.gcc_relocates_pch_on_windows(version) is expected


def test_gcc_version_asks_the_compiler() -> None:
    cxx = (sys.executable, "-c", "print('14.4.0')")
    assert pch.gcc_version(cxx) == (14, 4, 0)
    assert pch.gcc_version((sys.executable, "-c", "print('gcc')")) == ()
    assert pch.gcc_version(("/nonexistent/g++",)) == ()


def test_pch_usable_asks_the_compiler_on_windows_only(
    monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    caplog.set_level(logging.INFO, logger=pch.__name__)
    monkeypatch.delenv("ESPHOME_PCH_ENABLE")
    monkeypatch.setattr(pch.sys, "platform", "darwin")
    with patch.object(pch, "gcc_version", side_effect=AssertionError("off Windows")):
        assert pch.pch_usable(("g++",))
    monkeypatch.setattr(pch.sys, "platform", "win32")
    with patch.object(pch, "gcc_version", return_value=(14, 2, 0)):
        assert not pch.pch_usable(("g++",))
    assert "GCC 14.2.0 cannot load a precompiled header on Windows" in caplog.text
    with patch.object(pch, "gcc_version", return_value=(14, 4, 0)):
        assert pch.pch_usable(("g++",))
    # The knob overrides the rule both ways
    with patch.object(pch, "gcc_version", side_effect=AssertionError("forced")):
        monkeypatch.setenv("ESPHOME_PCH_ENABLE", "1")
        assert pch.pch_usable(("g++",))
        monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
        assert not pch.pch_usable(("g++",))


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
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: platform}
    assert pch.pch_script_enabled()
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
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
