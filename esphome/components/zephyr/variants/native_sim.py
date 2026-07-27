import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_MAC_ADDRESS,
    KEY_FRAMEWORK_VERSION,
    ThreadModel,
)
from esphome.types import ConfigType

from ..const import ZEPHYR_VARIANT_NATIVE_SIM
from . import MAINLINE, ZephyrVariant, resolve_framework_version, set_core_data

_DEFAULT_BOARD = "native_sim/native/64"
_VALID_BOARDS = [_DEFAULT_BOARD]

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_NATIVE_SIM
VARIANT = ZephyrVariant(
    sdk=MAINLINE,
    boards=_VALID_BOARDS,
    valid_toolchains=("sdk-zephyr",),
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_MAC_ADDRESS not in config:
        config[CONF_MAC_ADDRESS] = cv.mac_address("98:35:69:ab:f6:79")
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    board = config[CONF_BOARD]
    if board not in _VALID_BOARDS:
        raise cv.Invalid(
            f"Board {board!r} is not supported by native_sim. "
            f"Supported board: {_DEFAULT_BOARD!r}",
            [CONF_BOARD],
        )
    version_str, framework_ver = resolve_framework_version(
        VARIANT, "native_sim", config, "native_sim support"
    )
    set_core_data(VARIANT_NAME, board, "", framework_ver, config)
    config[KEY_FRAMEWORK_VERSION] = version_str
    return config


async def to_code(config: ConfigType) -> None:
    from .. import zephyr_add_prj_conf, zephyr_setup_preferences, zephyr_to_code

    zephyr_to_code(config)
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_NATIVE_SIM")
    cg.add_define("ESPHOME_BOARD", ZEPHYR_VARIANT_NATIVE_SIM)
    cg.add_define("USE_ESPHOME_HOST_MAC_ADDRESS", config[CONF_MAC_ADDRESS].parts)
    cg.add_define(ThreadModel.MULTI_ATOMICS)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    zephyr_add_prj_conf("HEAP_MEM_POOL_SIZE", 65536)
