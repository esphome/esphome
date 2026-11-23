"""The shared zephyr_to_code sets only what every Zephyr chip can take."""

from esphome.components.zephyr import zephyr_data, zephyr_set_core_data, zephyr_to_code
from esphome.components.zephyr.const import (
    BOOTLOADER_MCUBOOT,
    KEY_BOOTLOADER,
    KEY_PRJ_CONF,
)
from esphome.const import CONF_BOARD
from esphome.core import CORE


def test_shared_base_leaves_fpu_and_entropy_to_the_platform() -> None:
    """Chips without an FPU or an RNG build on the shared base too."""
    CORE.config = {}
    zephyr_set_core_data({CONF_BOARD: "xiao_ble", KEY_BOOTLOADER: BOOTLOADER_MCUBOOT})

    zephyr_to_code({})

    prj_conf = zephyr_data()[KEY_PRJ_CONF][""]
    assert "CONFIG_FPU" not in prj_conf
    assert "CONFIG_ENTROPY_GENERATOR" not in prj_conf
