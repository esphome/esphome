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


def test_ccache_pch_env_empty_until_emitted() -> None:
    """No sloppiness relaxation for a build that skipped the pch."""
    with patch.dict(os.environ, {}, clear=True):
        assert pch.ccache_pch_env() == {}


def test_ccache_pch_env_enabled() -> None:
    pch.mark_pch_emitted()
    with patch.dict(os.environ, {}, clear=True):
        env = pch.ccache_pch_env()
    assert env["CCACHE_SLOPPINESS"] == "pch_defines,time_macros"
    assert env["CCACHE_PCH_EXTSUM"] == "true"


def test_ccache_pch_env_disabled() -> None:
    with patch.dict(os.environ, {"ESPHOME_PCH_ENABLE": "0"}, clear=True):
        assert pch.ccache_pch_env() == {}


def test_ccache_pch_env_token_check_is_membership_not_substring(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A token merely containing ours must not suppress the union."""
    pch.mark_pch_emitted()
    with patch.dict(os.environ, {"CCACHE_SLOPPINESS": "pch_defines_extra"}, clear=True):
        env = pch.ccache_pch_env()
    assert env["CCACHE_SLOPPINESS"] == "pch_defines_extra,pch_defines,time_macros"


def test_ccache_pch_env_unions_user_sloppiness(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Without pch_defines/time_macros ccache declines every pch-consuming
    compile, so missing tokens are unioned onto the user's value."""
    pch.mark_pch_emitted()
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


def test_pch_extra_scripts_gated(monkeypatch: pytest.MonkeyPatch) -> None:
    with patch.dict(os.environ, {}, clear=True):
        assert pch.pch_extra_scripts() == ["post:pch.py"]
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    assert pch.pch_extra_scripts() == []


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


def test_ccache_pch_env_warns_on_falsy_extsum(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A user CCACHE_PCH_EXTSUM=false makes ccache hash the .gch bytes."""
    pch.mark_pch_emitted()
    with patch.dict(os.environ, {"CCACHE_PCH_EXTSUM": "false"}, clear=True):
        env = pch.ccache_pch_env()
    assert "CCACHE_PCH_EXTSUM" not in env
    assert "disables pch caching" in caplog.text


@pytest.mark.parametrize(
    ("value", "expected"),
    [(None, False), ("0", False), ("1", True), ("true", True)],
)
def test_pch_strict(
    value: str | None, expected: bool, monkeypatch: pytest.MonkeyPatch
) -> None:
    if value is None:
        monkeypatch.delenv("ESPHOME_PCH_STRICT", raising=False)
    else:
        monkeypatch.setenv("ESPHOME_PCH_STRICT", value)
    assert pch.pch_strict() is expected


def test_pch_degraded_raises_only_in_strict(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    from esphome.core import EsphomeError

    monkeypatch.delenv("ESPHOME_PCH_STRICT", raising=False)
    pch.pch_degraded("reason")
    monkeypatch.setenv("ESPHOME_PCH_STRICT", "1")
    with pytest.raises(EsphomeError, match="reason"):
        pch.pch_degraded("reason")


def test_pch_extra_scripts_strict_raises_when_disabled(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    from esphome.core import EsphomeError

    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    monkeypatch.setenv("ESPHOME_PCH_STRICT", "1")
    with pytest.raises(EsphomeError, match="disabled"):
        pch.pch_extra_scripts()


def test_pch_strict_rejects_unrecognized_values(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A typo must not silently disable the gate."""
    from esphome.core import EsphomeError

    monkeypatch.setenv("ESPHOME_PCH_STRICT", "yolo")
    with pytest.raises(EsphomeError, match="Unrecognized ESPHOME_PCH_STRICT"):
        pch.pch_strict()


def test_discard_pch_raises_when_gch_survives(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A .gch an unlink failure leaves behind would be consumed silently."""
    from pathlib import Path as _P

    from esphome.core import EsphomeError

    (tmp_path / "esphome_pch.h").write_text("")
    gch = tmp_path / "esphome_pch.h.gch"
    gch.write_bytes(b"gch")
    real_unlink = _P.unlink

    def failing_unlink(self, missing_ok=False):
        if self.name.endswith(".gch"):
            raise OSError("readonly")
        return real_unlink(self, missing_ok=missing_ok)

    monkeypatch.setattr(_P, "unlink", failing_unlink)
    with pytest.raises(EsphomeError, match="Could not discard"):
        pch.discard_pch(tmp_path)


def test_discard_pch_raises_when_sum_survives(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """The .sum is ccache's pch identity; one that survives is as unsafe
    as a surviving .gch."""
    from pathlib import Path as _P

    from esphome.core import EsphomeError

    (tmp_path / "esphome_pch.h").write_text("")
    (tmp_path / "esphome_pch.h.gch").write_bytes(b"gch")
    (tmp_path / "esphome_pch.h.gch.sum").write_text("x")
    real_unlink = _P.unlink

    def failing_unlink(self, missing_ok=False):
        if self.name.endswith(".sum"):
            raise OSError("readonly")
        return real_unlink(self, missing_ok=missing_ok)

    monkeypatch.setattr(_P, "unlink", failing_unlink)
    with pytest.raises(EsphomeError, match="Could not discard"):
        pch.discard_pch(tmp_path)


def test_discard_pch_warns_when_file_vanished_concurrently(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """An unlink error on a file that is nonetheless gone only warns."""
    from pathlib import Path as _P

    (tmp_path / "esphome_pch.h").write_text("")
    (tmp_path / "esphome_pch.h.gch").write_bytes(b"gch")
    real_unlink = _P.unlink

    def racing_unlink(self, missing_ok=False):
        if self.name.endswith(".sum"):
            # Racer removed it, then our unlink errored
            raise OSError("stale handle")
        return real_unlink(self, missing_ok=missing_ok)

    monkeypatch.setattr(_P, "unlink", racing_unlink)
    pch.discard_pch(tmp_path)
    assert "Could not discard the pch sidecars" in caplog.text
