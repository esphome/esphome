from esphome.build_helpers.pch import PCH_PREFIX_HEADER, pch_enabled, pch_extra_scripts
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
)
from esphome.core import CORE
from esphome.platformio.toolchain import copy_ccache_script, copy_pch_script
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
    cv.require_platformio_toolchain("host"),
    set_core_data,
)


async def to_code(config: ConfigType) -> None:
    cg.add_build_flag("-DUSE_HOST")
    cg.add_define("USE_NATIVE_64BIT_TIME")
    # The prefs file finds stored preferences by key, so key migration is possible
    cg.add_define("USE_PREFERENCE_KEY_LOOKUP")
    cg.add_define("USE_ESPHOME_HOST_MAC_ADDRESS", config[CONF_MAC_ADDRESS].parts)
    cg.add_build_flag("-std=gnu++20")
    cg.add_define("ESPHOME_BOARD", "host")
    cg.add_define("ESPHOME_VARIANT", "HOST")
    cg.add_define(ThreadModel.MULTI_ATOMICS)
    cg.add_platformio_option("platform", "platformio/native")
    cg.add_platformio_option("lib_ldf_mode", "off")
    cg.add_platformio_option("lib_compat_mode", "strict")
    cg.add_platformio_option("extra_scripts", ["pre:ccache.py", *pch_extra_scripts()])
    if pch_enabled():
        # Curated prefix for the pch (the script folds it plus defines.h):
        # host has no framework force-includes, and the per-TU cost is the
        # STL closure behind the core headers. Measured -43% compile CPU.
        # Gated so ESPHOME_PCH_ENABLE=0 restores the strict view. When the
        # .gch fails to build or load, the force-include stays and every TU
        # parses the closure as text: correct, but slower than no pch.
        cg.add_platformio_option("build_src_flags", f"-include {PCH_PREFIX_HEADER}")


# Called by writer.py
def copy_files() -> None:
    copy_ccache_script()
    copy_pch_script()
