"""Unit tests for esphome.components.zephyr.framework_west."""

from __future__ import annotations

from pathlib import Path

import platformdirs
import pytest

from esphome.components.zephyr.framework_west import get_zephyr_tools_path

# ---------------------------------------------------------------------------
# get_zephyr_tools_path -- machine-global cache location
# ---------------------------------------------------------------------------


def test_tools_path_env_override(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    override = tmp_path / "custom" / "zephyr"
    monkeypatch.setenv("ESPHOME_ZEPHYR_PREFIX", str(override))
    assert get_zephyr_tools_path() == override.resolve()


def test_tools_path_default_is_global_cache(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("ESPHOME_ZEPHYR_PREFIX", raising=False)
    expected = (
        Path(platformdirs.user_cache_dir("esphome", appauthor=False)) / "zephyr"
    ).resolve()
    assert get_zephyr_tools_path() == expected
