"""Tests for the west build code nrf52 and the Zephyr toolchain share."""

from pathlib import Path

import pytest

from esphome.components.zephyr.build_zephyr import clean_changed_build


@pytest.mark.parametrize(
    ("has_cache", "cmake_lists_changed", "wiped"),
    [
        pytest.param(True, False, False, id="unchanged"),
        pytest.param(True, True, True, id="cmake_lists_changed"),
        pytest.param(False, False, True, id="cache_dropped"),
    ],
)
def test_clean_changed_build(
    tmp_path: Path, has_cache: bool, cmake_lists_changed: bool, wiped: bool
) -> None:
    build_dir = tmp_path / "build"
    (build_dir / "zephyr").mkdir(parents=True)
    if has_cache:
        (build_dir / "CMakeCache.txt").write_text("")

    clean_changed_build(build_dir, cmake_lists_changed)

    assert build_dir.exists() is not wiped


def test_clean_changed_build_without_a_build(tmp_path: Path) -> None:
    """A first build has nothing to clean."""
    clean_changed_build(tmp_path / "build", True)

    assert not (tmp_path / "build").exists()
