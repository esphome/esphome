"""Unit tests for esphome.components.zephyr.dts_fetch."""

from __future__ import annotations

from pathlib import Path
import subprocess
from unittest.mock import patch

from esphome.components.zephyr.dts_fetch import (
    _boards_clone_tag,
    _framework_base_version,
    _sparse_clone_dts,
)
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE


def _set_framework_version(major: int, minor: int, patch: int) -> None:
    CORE.data[KEY_CORE] = {KEY_FRAMEWORK_VERSION: cv.Version(major, minor, patch)}


# ---------------------------------------------------------------------------
# _framework_base_version
# ---------------------------------------------------------------------------


def test_framework_base_version_formats_major_minor_patch() -> None:
    _set_framework_version(4, 4, 1)
    assert _framework_base_version() == "4.4.1"


# ---------------------------------------------------------------------------
# _boards_clone_tag
# ---------------------------------------------------------------------------


def test_boards_clone_tag_prefixes_v() -> None:
    assert _boards_clone_tag("ESP32H2", "4.4.1") == "v4.4.1"


# ---------------------------------------------------------------------------
# _sparse_clone_dts -- regression coverage for the cone-mode VERSION bug
# ---------------------------------------------------------------------------


def test_sparse_clone_dts_returns_none_for_unregistered_variant() -> None:
    _set_framework_version(4, 4, 1)
    assert _sparse_clone_dts("NOT_A_REAL_VARIANT") is None


def test_sparse_clone_dts_sparse_checkout_never_lists_version_file(
    tmp_path: Path,
) -> None:
    """Regression test: cone-mode `git sparse-checkout set` only accepts directory
    patterns. VERSION is a top-level file in the zephyr repo -- passing it as a
    pattern fails the whole fetch ("'VERSION' is not a directory"). Cone mode
    already includes top-level files automatically, so it must never be listed.
    """
    _set_framework_version(4, 4, 1)

    calls: list[list[str]] = []

    def fake_run(cmd, **kwargs):
        calls.append(cmd)
        result = subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")
        if cmd[:2] == ["git", "clone"]:
            # Simulate the clone creating the destination directory tree so the
            # post-clone sparse-checkout step has somewhere to run "in".
            dest = Path(cmd[-1])
            (dest / "boards").mkdir(parents=True, exist_ok=True)
        return result

    with (
        patch(
            "esphome.components.zephyr.dts_fetch._DTS_CACHE",
            tmp_path / "zephyr_dts_cache",
        ),
        patch("subprocess.run", side_effect=fake_run),
    ):
        result = _sparse_clone_dts("ESP32H2")

    sparse_checkout_calls = [c for c in calls if "sparse-checkout" in c]
    assert len(sparse_checkout_calls) == 1
    assert "VERSION" not in sparse_checkout_calls[0]
    assert result is not None


def test_sparse_clone_dts_returns_none_and_cleans_up_on_git_failure(
    tmp_path: Path,
) -> None:
    _set_framework_version(4, 4, 1)

    def fake_run(cmd, **kwargs):
        raise subprocess.CalledProcessError(1, cmd, stderr="fatal: some git error")

    dest = tmp_path / "zephyr_dts_cache" / "ESP32H2" / "4.4.1"
    with (
        patch(
            "esphome.components.zephyr.dts_fetch._DTS_CACHE",
            tmp_path / "zephyr_dts_cache",
        ),
        patch("subprocess.run", side_effect=fake_run),
    ):
        result = _sparse_clone_dts("ESP32H2")

    assert result is None
    assert not dest.exists()  # cleaned up, not left in a half-cloned state


def test_sparse_clone_dts_returns_cached_path_without_reinvoking_git(
    tmp_path: Path,
) -> None:
    _set_framework_version(4, 4, 1)
    dest = tmp_path / "zephyr_dts_cache" / "ESP32H2" / "4.4.1"
    (dest / "boards").mkdir(parents=True)

    with (
        patch(
            "esphome.components.zephyr.dts_fetch._DTS_CACHE",
            tmp_path / "zephyr_dts_cache",
        ),
        patch("subprocess.run") as mock_run,
    ):
        result = _sparse_clone_dts("ESP32H2")

    mock_run.assert_not_called()
    assert result == dest
