import os.path
import subprocess

from esphome.const import (
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_HOST,
    CONF_MAC_ADDRESS,
)
from esphome.core import CORE, EsphomeError
from esphome.helpers import IS_MACOS, IS_LINUX
from esphome.components.api import CONF_ENCRYPTION
import esphome.config_validation as cv
import esphome.codegen as cg

from .const import KEY_HOST

# force import gpio to register pin schema
from .gpio import host_pin_to_code  # noqa

CODEOWNERS = ["@esphome/core", "@clydebarrow"]
AUTO_LOAD = ["network"]


def set_core_data(config):
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
    set_core_data,
)

MAC_LIBSODIUM = ("/opt/homebrew/lib/libsodium.a", "/usr/local/lib/libsodium.a")


def libsodium():
    if api_conf := CORE.config.get("api"):
        if CONF_ENCRYPTION in api_conf:
            if IS_MACOS:
                files = list(filter(os.path.isfile, MAC_LIBSODIUM))
                if not files:
                    raise EsphomeError(
                        "libsodium required for api encryption - install with `brew install libsodium'"
                    )
                # Apple clang does not find this library with -lsodium apparently because there is already a libsodium.a
                # in the build tree, linked explicitly.
                cg.add_build_flag(files[0])
            elif IS_LINUX:
                try:
                    with subprocess.Popen(
                        ("apt", "-qq", "list", "libsodium-dev"),
                        stderr=subprocess.DEVNULL,
                        stdout=subprocess.PIPE,
                    ).stdout as stdout:
                        sodium = str(stdout.read())
                        if "libsodium-dev" in sodium and "installed" not in sodium:
                            raise EsphomeError(
                                "libsodium required for api encryption - install with `sudo apt install libsodium-dev'"
                            )
                except FileNotFoundError:
                    pass
                cg.add_build_flag("-lsodium")
            else:
                # How to check on Windows? Who knows.
                cg.add_build_flag("-lsodium")


async def to_code(config):
    libsodium()
    cg.add_build_flag("-DUSE_HOST")
    cg.add_define("USE_ESPHOME_HOST_MAC_ADDRESS", config[CONF_MAC_ADDRESS].parts)
    cg.add_build_flag("-std=c++17")
    cg.add_define("ESPHOME_BOARD", "host")
    cg.add_platformio_option("platform", "platformio/native")
