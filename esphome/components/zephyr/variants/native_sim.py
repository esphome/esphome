import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADVANCED,
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_MAC_ADDRESS,
    CONF_SOURCE,
    ThreadModel,
)
from esphome.types import ConfigType

from ..const import ADVANCED_SCHEMA, CONF_RUNNER, ZEPHYR_VARIANT_NATIVE_SIM
from . import MAINLINE, ZephyrVariant, resolve_framework_version, set_core_data

_DEFAULT_BOARD = "native_sim/native/64"
_VALID_BOARDS = [_DEFAULT_BOARD]

# advanced: mac_address: -- native_sim has no real network hardware to read a MAC
# from, so this is the only variant where it's user-settable at all.
_ADVANCED_SCHEMA = ADVANCED_SCHEMA.extend(
    {
        cv.Optional(CONF_MAC_ADDRESS, default="98:35:69:ab:f6:79"): cv.mac_address,
    }
)

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_NATIVE_SIM
VARIANT = ZephyrVariant(
    sdk=MAINLINE,
    sdk_name="zephyr",
    boards=_VALID_BOARDS,
    valid_toolchains=("sdk-zephyr",),
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_ADVANCED] = _ADVANCED_SCHEMA(config.get(CONF_ADVANCED, {}))
    board = config[CONF_BOARD]
    if board not in _VALID_BOARDS:
        raise cv.Invalid(
            f"Board {board!r} is not supported by native_sim. "
            f"Supported board: {_DEFAULT_BOARD!r}",
            [CONF_BOARD],
        )
    _, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "native_sim", config, "native_sim support"
    )
    set_core_data(
        VARIANT_NAME,
        board,
        "",
        framework_ver,
        config,
        framework_type=sdk_name,
        sdk_source=config[CONF_FRAMEWORK].get(CONF_SOURCE),
        runner=config[CONF_ADVANCED].get(CONF_RUNNER),
    )
    return config


async def to_code(config: ConfigType) -> None:
    from .. import zephyr_add_prj_conf, zephyr_setup_preferences, zephyr_to_code

    zephyr_to_code(config)
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_NATIVE_SIM")
    cg.add_define("ESPHOME_BOARD", ZEPHYR_VARIANT_NATIVE_SIM)
    cg.add_define(
        "USE_ESPHOME_HOST_MAC_ADDRESS", config[CONF_ADVANCED][CONF_MAC_ADDRESS].parts
    )
    cg.add_define(ThreadModel.MULTI_ATOMICS)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    zephyr_add_prj_conf("HEAP_MEM_POOL_SIZE", 65536)
