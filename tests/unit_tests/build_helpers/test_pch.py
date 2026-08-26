"""Tests for esphome.build_helpers.pch."""

from __future__ import annotations

import os
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.build_helpers import pch


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
    assert env["CCACHE_SLOPPINESS"] == "pch_defines,time_macros"
    assert env["CCACHE_PCH_EXTSUM"] == "true"


def test_ccache_pch_env_disabled() -> None:
    with patch.dict(os.environ, {"ESPHOME_PCH_ENABLE": "0"}, clear=True):
        assert pch.ccache_pch_env() == {}


def test_ccache_pch_env_unions_user_sloppiness(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Without pch_defines/time_macros ccache declines every pch-consuming
    compile, so missing tokens are unioned onto the user's value."""
    with patch.dict(os.environ, {"CCACHE_SLOPPINESS": "locale"}, clear=True):
        env = pch.ccache_pch_env()
    assert env["CCACHE_SLOPPINESS"] == "locale,pch_defines,time_macros"
    assert env["CCACHE_PCH_EXTSUM"] == "true"
    assert "Adding pch_defines,time_macros" in caplog.text
    caplog.clear()
    with patch.dict(
        os.environ, {"CCACHE_SLOPPINESS": "pch_defines,time_macros"}, clear=True
    ):
        env = pch.ccache_pch_env()
    assert "CCACHE_SLOPPINESS" not in env
    assert not caplog.records


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
def test_include_closure_marks_unreadable(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """An unreadable header warns and hashes as a marker, so it still
    invalidates instead of silently vanishing from the digest."""
    _write(tmp_path, "a.h", '#include "locked.h"\n')
    locked = tmp_path / "locked.h"
    locked.write_text("")
    locked.chmod(0)
    try:
        closure = pch._include_closure(tmp_path, ["a.h"])
    finally:
        locked.chmod(0o644)
    # stat still works, so the marker varies with mtime/size and a later
    # edit to the unreadable file still shifts the digest
    assert closure["locked.h"].startswith(b"<unreadable:")
    assert "Could not read locked.h" in caplog.text


def test_pch_extra_scripts_gated(monkeypatch: pytest.MonkeyPatch) -> None:
    with patch.dict(os.environ, {}, clear=True):
        assert pch.pch_extra_scripts() == ["post:pch.py"]
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    assert pch.pch_extra_scripts() == []


def test_include_closure_raises_when_identity_unknown(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Read AND stat failing means no marker can vouch for the header, so
    the OSError propagates and callers compile without a pch."""

    class _BadFile:
        def is_file(self) -> bool:
            return True

        def read_bytes(self) -> bytes:
            raise OSError("read failed")

        def stat(self) -> None:
            raise OSError("stat failed")

    class _FakeSrcDir:
        def __truediv__(self, rel: str) -> _BadFile:
            return _BadFile()

    with pytest.raises(OSError, match="stat failed"):
        pch._include_closure(_FakeSrcDir(), ["a.h"])
    assert "Could not read a.h" in caplog.text


def test_include_closure_survives_non_utf8_include_name(tmp_path: Path) -> None:
    """A non-UTF-8 quoted include must not abort the build; it simply does
    not resolve and ends the walk."""
    (tmp_path / "a.h").write_bytes(b'#include "bad\xff.h"\n#include "b.h"\n')
    (tmp_path / "b.h").write_text("")
    closure = pch._include_closure(tmp_path, ["a.h"])
    assert set(closure) == {"a.h", "b.h"}
