"""STM32C5 chips."""

import esphome.codegen as cg
from esphome.components.zephyr.variants import (
    ZEPHYR_MAINLINE,
    ZephyrVariant,
    resolve_framework_version,
    set_core_data,
)
from esphome.const import CONF_BOARD, ThreadModel
from esphome.types import ConfigType

from ..const import VARIANT_STM32C5

# The largest C5 Nucleo (1M flash, 256K RAM)
_DEFAULT_BOARD: str = "nucleo_c5a3zg"

VARIANT_NAME: str = VARIANT_STM32C5
VARIANT: ZephyrVariant = ZephyrVariant(sdk=ZEPHYR_MAINLINE, family="stm32")


def config_schema(config: ConfigType) -> ConfigType:
    config = {CONF_BOARD: _DEFAULT_BOARD, **config}
    _, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, VARIANT_NAME, config, "mainline STM32 support"
    )
    set_core_data(
        VARIANT_NAME, config, framework_ver, framework_type=sdk_name, bootloader=""
    )
    return config


def to_code(config: ConfigType) -> None:
    # deferred: zephyr's __init__ may load the variants while it imports
    from esphome.components.zephyr import zephyr_add_prj_conf, zephyr_setup_preferences

    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", VARIANT_NAME)
    cg.add_define(ThreadModel.MULTI_ATOMICS)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
