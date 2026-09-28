"""Tests for esphome.build_helpers.pch."""

from __future__ import annotations

import os
from pathlib import Path
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


def test_ccache_pch_env_enabled() -> None:
    with patch.dict(os.environ, {}, clear=True):
        env = pch.ccache_pch_env()
    assert env == pch.CCACHE_PCH_ENV


def test_ccache_pch_env_user_values_win() -> None:
    with patch.dict(os.environ, {"CCACHE_SLOPPINESS": "locale"}, clear=True):
        assert pch.ccache_pch_env() == {"CCACHE_PCH_EXTSUM": "true"}


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


@pytest.mark.parametrize("platform", ["esp32", "esp8266", "rp2"])
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


def test_pch_cmake_consumer_substitutes_target_and_sources(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    block = pch.pch_cmake_consumer("app", "${APP_SOURCES}")
    assert "target_compile_options(app PRIVATE" in block
    assert '"$<$<COMPILE_LANGUAGE:CXX>:-Winvalid-pch>"' in block
    assert '"$<$<COMPILE_LANGUAGE:CXX>:-Werror=invalid-pch>"' in block
    assert '"$<$<COMPILE_LANGUAGE:CXX>:esphome_pch.h>"' in block
    assert "set_source_files_properties(${APP_SOURCES} PROPERTIES" in block
    assert 'OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/esphome_pch.h"' in block


def test_pch_cmake_consumer_empty_when_disabled(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    assert pch.pch_cmake_consumer("app", "${APP_SOURCES}") == ""
