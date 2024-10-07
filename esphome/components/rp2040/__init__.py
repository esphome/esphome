import logging
import os
from string import ascii_letters, digits

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_PLATFORM_VERSION,
    CONF_SOURCE,
    CONF_VERSION,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_RP2040,
)
from esphome.core import CORE, EsphomeError, coroutine_with_priority
from esphome.helpers import copy_file_if_changed, mkdir_p, write_file

from .const import KEY_BOARD, KEY_PIO_FILES, KEY_RP2040, rp2040_ns

# force import gpio to register pin schema
from .gpio import rp2040_pin_to_code  # noqa

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@jesserockz"]
AUTO_LOAD = []


def set_core_data(config):
    CORE.data[KEY_RP2040] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_RP2040
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "arduino"
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version.parse(
        config[CONF_FRAMEWORK][CONF_VERSION]
    )
    CORE.data[KEY_RP2040][KEY_BOARD] = config[CONF_BOARD]

    CORE.data[KEY_RP2040][KEY_PIO_FILES] = {}

    return config


def get_download_types(storage_json):
    return [
        {
            "title": "UF2 factory format",
            "description": "For copying to RP2040 over USB.",
            "file": "firmware.uf2",
            "download": f"{storage_json.name}.factory.uf2",
        },
        {
            "title": "OTA format",
            "description": "For OTA updating a device.",
            "file": "firmware.ota.bin",
            "download": f"{storage_json.name}.ota.bin",
        },
    ]


def _format_framework_arduino_version(ver: cv.Version) -> str:
    # The most recent releases have not been uploaded to platformio so grabbing them directly from
    # the GitHub release is one path forward for now.
    return f"https://github.com/earlephilhower/arduino-pico/releases/download/{ver}/rp2040-{ver}.zip"

    # format the given arduino (https://github.com/earlephilhower/arduino-pico/releases) version to
    # a PIO earlephilhower/framework-arduinopico value
    # List of package versions: https://api.registry.platformio.org/v3/packages/earlephilhower/tool/framework-arduinopico
    # return f"~1.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"


def _parse_platform_version(value):
    value = cv.string(value)
    if value.startswith("http"):
        return value

    return f"https://github.com/maxgerhardt/platform-raspberrypi.git#{value}"


# NOTE: Keep this in mind when updating the recommended version:
#  * The new version needs to be thoroughly validated before changing the
#    recommended version as otherwise a bunch of devices could be bricked
#  * For all constants below, update platformio.ini (in this repo)
#    and platformio.ini/platformio-lint.ini in the esphome-docker-base repository

# The default/recommended arduino framework version
#  - https://github.com/earlephilhower/arduino-pico/releases
#  - https://api.registry.platformio.org/v3/packages/earlephilhower/tool/framework-arduinopico
RECOMMENDED_ARDUINO_FRAMEWORK_VERSION = cv.Version(3, 9, 4)

# The raspberrypi platform version to use for arduino frameworks
#  - https://github.com/maxgerhardt/platform-raspberrypi/tags
RECOMMENDED_ARDUINO_PLATFORM_VERSION = "v1.2.0-gcc12"


def _arduino_check_versions(value):
    value = value.copy()
    lookups = {
        "dev": (cv.Version(3, 9, 4), "https://github.com/earlephilhower/arduino-pico"),
        "latest": (cv.Version(3, 9, 4), None),
        "recommended": (RECOMMENDED_ARDUINO_FRAMEWORK_VERSION, None),
    }

    if value[CONF_VERSION] in lookups:
        if CONF_SOURCE in value:
            raise cv.Invalid(
                "Framework version needs to be explicitly specified when custom source is used."
            )

        version, source = lookups[value[CONF_VERSION]]
    else:
        version = cv.Version.parse(cv.version_number(value[CONF_VERSION]))
        source = value.get(CONF_SOURCE, None)

    value[CONF_VERSION] = str(version)
    value[CONF_SOURCE] = source or _format_framework_arduino_version(version)

    value[CONF_PLATFORM_VERSION] = value.get(
        CONF_PLATFORM_VERSION,
        _parse_platform_version(RECOMMENDED_ARDUINO_PLATFORM_VERSION),
    )

    if version != RECOMMENDED_ARDUINO_FRAMEWORK_VERSION:
        _LOGGER.warning(
            "The selected Arduino framework version is not the recommended one."
        )

    return value


ARDUINO_FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_VERSION, default="recommended"): cv.string_strict,
            cv.Optional(CONF_SOURCE): cv.string_strict,
            cv.Optional(CONF_PLATFORM_VERSION): _parse_platform_version,
        }
    ),
    _arduino_check_versions,
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_BOARD): cv.string_strict,
            cv.Optional(CONF_FRAMEWORK, default={}): ARDUINO_FRAMEWORK_SCHEMA,
        }
    ),
    set_core_data,
)


@coroutine_with_priority(1000)
async def to_code(config):
    cg.add(rp2040_ns.setup_preferences())

    # Allow LDF to properly discover dependency including those in preprocessor
    # conditionals
    cg.add_platformio_option("lib_ldf_mode", "chain+")
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_RP2040")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "RP2040")

    cg.add_platformio_option("extra_scripts", ["post:post_build.py"])

    conf = config[CONF_FRAMEWORK]
    cg.add_platformio_option("framework", "arduino")
    cg.add_build_flag("-DUSE_ARDUINO")
    cg.add_build_flag("-DUSE_RP2040_FRAMEWORK_ARDUINO")
    # cg.add_build_flag("-DPICO_BOARD=pico_w")
    cg.add_platformio_option("platform", conf[CONF_PLATFORM_VERSION])
    cg.add_platformio_option(
        "platform_packages",
        [
            f"earlephilhower/framework-arduinopico@{conf[CONF_SOURCE]}",
        ],
    )

    cg.add_platformio_option("board_build.core", "earlephilhower")
    cg.add_platformio_option("board_build.filesystem_size", "1m")

    ver: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    cg.add_define(
        "USE_ARDUINO_VERSION_CODE",
        cg.RawExpression(f"VERSION_CODE({ver.major}, {ver.minor}, {ver.patch})"),
    )


def add_pio_file(component: str, key: str, data: str):
    try:
        cv.validate_id_name(key)
    except cv.Invalid as e:
        raise EsphomeError(
            f"[{component}] Invalid PIO key: {key}. Allowed characters: [{ascii_letters}{digits}_]\nPlease report an issue https://github.com/esphome/issues"
        ) from e
    CORE.data[KEY_RP2040][KEY_PIO_FILES][key] = data


def generate_pio_files() -> bool:
    import shutil

    shutil.rmtree(CORE.relative_build_path("src/pio"), ignore_errors=True)

    includes: list[str] = []
    files = CORE.data[KEY_RP2040][KEY_PIO_FILES]
    if not files:
        return False
    for key, data in files.items():
        pio_path = CORE.relative_build_path(f"src/pio/{key}.pio")
        mkdir_p(os.path.dirname(pio_path))
        write_file(pio_path, data)
        includes.append(f"pio/{key}.pio.h")

    write_file(
        CORE.relative_build_path("src/pio_includes.h"),
        "#pragma once\n" + "\n".join([f'#include "{include}"' for include in includes]),
    )

    dir = os.path.dirname(__file__)
    build_pio_file = os.path.join(dir, "build_pio.py.script")
    copy_file_if_changed(
        build_pio_file,
        CORE.relative_build_path("build_pio.py"),
    )

    return True


# Called by writer.py
def copy_files() -> bool:
    dir = os.path.dirname(__file__)
    post_build_file = os.path.join(dir, "post_build.py.script")
    copy_file_if_changed(
        post_build_file,
        CORE.relative_build_path("post_build.py"),
    )
    return generate_pio_files()
