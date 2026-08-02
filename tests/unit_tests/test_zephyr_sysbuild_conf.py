"""Tests for sysbuild-level Kconfig options (SB_CONFIG_*)."""

import pytest

from esphome.components.zephyr import (
    HexValue,
    zephyr_add_sysbuild_conf,
    zephyr_data,
    zephyr_set_core_data,
)
from esphome.components.zephyr.const import (
    BOOTLOADER_MCUBOOT,
    KEY_BOOTLOADER,
    KEY_SYSBUILD_CONF,
)
from esphome.const import CONF_BOARD


@pytest.fixture(autouse=True)
def zephyr_core() -> None:
    zephyr_set_core_data({CONF_BOARD: "xiao_ble", KEY_BOOTLOADER: BOOTLOADER_MCUBOOT})


def test_prefix_is_added_when_missing() -> None:
    """A bare name is stored under its SB_CONFIG_ prefix."""
    zephyr_add_sysbuild_conf("MCUBOOT_MODE_OVERWRITE_ONLY", True)

    assert zephyr_data()[KEY_SYSBUILD_CONF] == {
        "SB_CONFIG_MCUBOOT_MODE_OVERWRITE_ONLY": True
    }


def test_prefix_is_not_doubled() -> None:
    """An already-prefixed name is stored as-is."""
    zephyr_add_sysbuild_conf("SB_CONFIG_BOOT_SIGNATURE_TYPE_NONE", True)

    assert "SB_CONFIG_BOOT_SIGNATURE_TYPE_NONE" in zephyr_data()[KEY_SYSBUILD_CONF]


def test_setting_the_same_value_twice_is_allowed() -> None:
    """Two components asking for the same option agree, so it is not an error."""
    zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_NONE", True)
    zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_NONE", True)

    assert zephyr_data()[KEY_SYSBUILD_CONF]["SB_CONFIG_BOOT_SIGNATURE_TYPE_NONE"] is True


def test_conflicting_value_raises() -> None:
    """A silent override here would change how every image is built."""
    zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_NONE", True)

    with pytest.raises(ValueError, match="already set"):
        zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_NONE", False)


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        (True, "y"),
        (False, "n"),
        (42, "42"),
        (HexValue(0xA000), "0xa000"),
        ("text", '"text"'),
    ],
)
def test_values_are_formatted_for_kconfig(value, expected: str) -> None:
    """sysbuild.conf uses the same value formatting as prj.conf."""
    from esphome.components.zephyr import _format_prj_conf_val

    assert _format_prj_conf_val(value) == expected
