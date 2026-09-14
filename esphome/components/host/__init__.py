import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAC_ADDRESS,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_HOST,
    ThreadModel,
    Toolchain,
)
from esphome.core import CORE, EsphomeError
from esphome.types import ConfigType

from .const import KEY_HOST

# force import gpio to register pin schema
from .gpio import host_pin_to_code  # noqa: F401

CODEOWNERS = ["@esphome/core", "@clydebarrow"]
AUTO_LOAD = ["network", "preferences"]
IS_TARGET_PLATFORM = True


def set_core_data(config: ConfigType) -> ConfigType:
    CORE.data[KEY_HOST] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_HOST
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "host"
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version(1, 0, 0)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_MAC_ADDRESS, default="98:35:69:ab:f6:79"): cv.mac_address,
        }
    ),
    # The host builds with the machine's own compiler through ninja; there
    # is no PlatformIO backend, so a CLI --toolchain must name this one
    cv.resolve_toolchain("host", (Toolchain.HOST,), Toolchain.HOST),
    set_core_data,
)


async def to_code(config: ConfigType) -> None:
    cg.add_build_flag("-DUSE_HOST")
    cg.add_define("USE_NATIVE_64BIT_TIME")
    # The prefs file finds stored preferences by key, so key migration is possible
    cg.add_define("USE_PREFERENCE_KEY_LOOKUP")
    cg.add_define("USE_ESPHOME_HOST_MAC_ADDRESS", config[CONF_MAC_ADDRESS].parts)
    cg.set_cpp_standard("gnu++20")
    cg.add_define("ESPHOME_BOARD", "host")
    cg.add_define("ESPHOME_VARIANT", "HOST")
    cg.add_define(ThreadModel.MULTI_ATOMICS)


# Called by __main__.compile_program; True means this platform built the
# program itself instead of falling through to the PlatformIO toolchain.
def run_compile(args: object, config: ConfigType) -> bool:
    from esphome.host import toolchain

    if toolchain.run_compile(config, CORE.verbose) != 0:
        raise EsphomeError("Host build failed")
    return True
