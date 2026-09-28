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


@pytest.mark.skipif(
    os.name == "nt" or os.geteuid() == 0, reason="chmod is ineffective here"
)
def test_include_closure_fails_closed_on_unreadable(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A marker would truncate the transitive walk; the OSError propagates
    so callers compile without a pch."""
    _write(tmp_path, "a.h", '#include "locked.h"\n')
    locked = tmp_path / "locked.h"
    locked.write_text("")
    locked.chmod(0)
    try:
        with pytest.raises(OSError):
            pch._include_closure(tmp_path, ["a.h"])
    finally:
        locked.chmod(0o644)
    assert "Could not read locked.h" in caplog.text


def test_pch_script_enabled(monkeypatch: pytest.MonkeyPatch) -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp8266"}
    assert pch.pch_script_enabled()
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    assert not pch.pch_script_enabled()


def test_pch_script_excluded_platform() -> None:
    excluded = next(iter(pch.PCH_SCRIPT_EXCLUDED_PLATFORMS))
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: excluded}
    assert not pch.pch_script_enabled()


def test_include_closure_raises_when_identity_unknown(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """An unreadable header propagates; callers compile without a pch."""

    class _BadFile:
        def stat(self):  # noqa: ANN202 -- regular-file mode only
            import os
            import stat as stat_mod

            return os.stat_result((stat_mod.S_IFREG | 0o644,) + (0,) * 9)

        def read_bytes(self) -> bytes:
            raise OSError("read failed")

    class _FakeSrcDir:
        def __truediv__(self, rel: str) -> _BadFile:
            return _BadFile()

    with pytest.raises(OSError, match="read failed"):
        pch._include_closure(_FakeSrcDir(), ["a.h"])
    assert "Could not read a.h" in caplog.text


def test_include_closure_survives_non_utf8_include_name(tmp_path: Path) -> None:
    """A non-UTF-8 quoted include must not abort the build; it simply does
    not resolve and ends the walk."""
    (tmp_path / "a.h").write_bytes(b'#include "bad\xff.h"\n#include "b.h"\n')
    (tmp_path / "b.h").write_text("")
    closure = pch._include_closure(tmp_path, ["a.h"])
    assert set(closure) == {"a.h", "b.h"}


def test_pch_checksum_survives_surrogate_extra(tmp_path: Path) -> None:
    """Install paths from non-UTF-8 filesystems carry surrogates; hashing
    them must not raise past the caller's identity-unknown guard."""
    assert pch.pch_checksum(tmp_path, [], ["/opt/bad\udcff/framework"])


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
    # Placeholder guard: survives a build-system-side pristine wipe
    assert 'file(TOUCH "${CMAKE_BINARY_DIR}/esphome_pch.h")' in block


def test_pch_cmake_consumer_empty_when_disabled(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    assert pch.pch_cmake_consumer("app", "${APP_SOURCES}") == ""
