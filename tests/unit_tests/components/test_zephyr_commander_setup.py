"""Unit tests for esphome.components.zephyr.commander_setup."""

from __future__ import annotations

from pathlib import Path

import platformdirs
import pytest

from esphome.components.zephyr.commander_setup import _install_dir

# ---------------------------------------------------------------------------
# _install_dir -- machine-global cache location
# ---------------------------------------------------------------------------


def test_install_dir_env_override(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    override = tmp_path / "custom" / "sdk-silabs"
    monkeypatch.setenv("ESPHOME_SDK_SILABS_PREFIX", str(override))
    assert _install_dir("1.24.1") == override.resolve() / "commander" / "1.24.1"


def test_install_dir_default_is_global_cache(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("ESPHOME_SDK_SILABS_PREFIX", raising=False)
    expected = (
        Path(platformdirs.user_cache_dir("esphome", appauthor=False))
        / "sdk-silabs"
        / "commander"
        / "1.24.1"
    ).resolve()
    assert _install_dir("1.24.1") == expected
