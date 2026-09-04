"""Unit tests for esphome.components.zephyr.sdk_setup_west."""

from __future__ import annotations

from pathlib import Path

import platformdirs
import pytest

from esphome.components.zephyr.sdk_setup_west import _sdk_install_dir

# ---------------------------------------------------------------------------
# _sdk_install_dir -- machine-global cache location
# ---------------------------------------------------------------------------


def test_sdk_install_dir_env_override(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    override = tmp_path / "custom" / "sdk-zephyr"
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(override))
    assert (
        _sdk_install_dir("0.17.4")
        == override.resolve() / "toolchains" / "zephyr-sdk-0.17.4"
    )


def test_sdk_install_dir_default_is_global_cache(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.delenv("ESPHOME_SDK_ZEPHYR_PREFIX", raising=False)
    expected = (
        Path(platformdirs.user_cache_dir("esphome", appauthor=False))
        / "sdk-zephyr"
        / "toolchains"
        / "zephyr-sdk-0.17.4"
    ).resolve()
    assert _sdk_install_dir("0.17.4") == expected
