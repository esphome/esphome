"""Tests for sysbuild-level Kconfig options (SB_CONFIG_*)."""

from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.components import zephyr
from esphome.components.zephyr import (
    zephyr_add_sysbuild_conf,
    zephyr_data,
    zephyr_set_core_data,
)
from esphome.components.zephyr.const import (
    BOOTLOADER_MCUBOOT,
    KEY_BOOTLOADER,
    KEY_SYSBUILD,
    KEY_SYSBUILD_CONF,
)
from esphome.const import CONF_BOARD
from esphome.core import CORE


@pytest.fixture(autouse=True)
def zephyr_core() -> None:
    zephyr_set_core_data({CONF_BOARD: "xiao_ble", KEY_BOOTLOADER: BOOTLOADER_MCUBOOT})


@pytest.mark.parametrize(
    "name", ["MCUBOOT_MODE_OVERWRITE_ONLY", "SB_CONFIG_MCUBOOT_MODE_OVERWRITE_ONLY"]
)
def test_prefix_is_added_once(name: str) -> None:
    zephyr_add_sysbuild_conf(name, True)

    assert zephyr_data()[KEY_SYSBUILD_CONF] == {
        "SB_CONFIG_MCUBOOT_MODE_OVERWRITE_ONLY": (True, True)
    }


def test_setting_an_option_enables_sysbuild() -> None:
    """Otherwise the option would never be written."""
    assert zephyr_data()[KEY_SYSBUILD] is False

    zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_NONE", True)

    assert zephyr_data()[KEY_SYSBUILD] is True


def test_setting_the_same_value_twice_is_allowed() -> None:
    zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_NONE", True)
    zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_NONE", True)

    assert zephyr_data()[KEY_SYSBUILD_CONF]["SB_CONFIG_BOOT_SIGNATURE_TYPE_NONE"] == (
        True,
        True,
    )


def test_conflicting_value_raises() -> None:
    zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_NONE", True)

    with pytest.raises(ValueError, match="already set"):
        zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_NONE", False)


def test_sysbuild_conf_is_written_with_the_bootloader(tmp_path: Path) -> None:
    CORE.build_path = tmp_path
    zephyr_add_sysbuild_conf("MCUBOOT_MODE_OVERWRITE_ONLY", True)

    with patch.object(zephyr, "clean_cmake_cache"):
        zephyr.copy_files()

    assert (tmp_path / "zephyr" / "sysbuild.conf").read_text() == (
        "SB_CONFIG_BOOTLOADER_MCUBOOT=y\nSB_CONFIG_MCUBOOT_MODE_OVERWRITE_ONLY=y\n"
    )
