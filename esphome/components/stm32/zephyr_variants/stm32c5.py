"""STM32C5 chips."""

import esphome.codegen as cg
from esphome.components.zephyr import zephyr_add_prj_conf, zephyr_setup_preferences
from esphome.components.zephyr.variants import (
    ZEPHYR_MAINLINE,
    ZephyrVariant,
    resolve_variant,
    set_variant_core_data,
)
from esphome.const import CONF_BOARD, ThreadModel
from esphome.types import ConfigType

from ..const import VARIANT_STM32C5
from . import stm32_family

# The largest C5 Nucleo (1M flash, 256K RAM)
_DEFAULT_BOARD = "nucleo_c5a3zg"

VARIANT: ZephyrVariant = ZephyrVariant(
    name=VARIANT_STM32C5, family_module=stm32_family, manifest=ZEPHYR_MAINLINE
)


def config_schema(config: ConfigType) -> ConfigType:
    config = {CONF_BOARD: _DEFAULT_BOARD, **config}
    resolved = resolve_variant(VARIANT, config, "mainline STM32 support")
    set_variant_core_data(resolved, config, bootloader="")
    return config


def to_code(config: ConfigType) -> None:
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", VARIANT.name)
    cg.add_define(ThreadModel.MULTI_ATOMICS)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
