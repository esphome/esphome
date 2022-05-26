import logging
import os

from esphome.helpers import copy_file_if_changed
from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_SOURCE,
    CONF_TYPE,
    CONF_VARIANT,
    CONF_VERSION,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    __version__,
)
from esphome.core import CORE
import esphome.config_validation as cv
import esphome.codegen as cg

from .const import (
    KEY_BOARD,
    KEY_LIBRETUYA,
    KEY_VARIANT,
    VARIANT_LIBRETUYA,
    VARIANTS,
)

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@esphome/core"]
AUTO_LOAD = ["preferences"]


def _set_core_data(config):
    CORE.data[KEY_LIBRETUYA] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = "libretuya"
    conf = config[CONF_FRAMEWORK]
    if conf[CONF_TYPE] == FRAMEWORK_ARDUINO:
        CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "arduino"
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version.parse(
        config[CONF_FRAMEWORK][CONF_VERSION]
    )
    CORE.data[KEY_LIBRETUYA][KEY_BOARD] = config[CONF_BOARD]
    CORE.data[KEY_LIBRETUYA][KEY_VARIANT] = config[CONF_VARIANT]
    return config


def _set_variant(value):
    value = value.copy()
    value[CONF_VARIANT] = VARIANT_LIBRETUYA
    return value


def get_libretuya_variant(core_obj=None):
    return (core_obj or CORE).data[KEY_LIBRETUYA][KEY_VARIANT]


def only_on_variant(*, supported=None, unsupported=None):
    """Config validator for features only available on some LibreTuya variants."""
    if supported is not None and not isinstance(supported, list):
        supported = [supported]
    if unsupported is not None and not isinstance(unsupported, list):
        unsupported = [unsupported]

    def validator_(obj):
        variant = get_libretuya_variant()
        if supported is not None and variant not in supported:
            raise cv.Invalid(
                f"This feature is only available on {', '.join(supported)}"
            )
        if unsupported is not None and variant in unsupported:
            raise cv.Invalid(
                f"This feature is not available on {', '.join(unsupported)}"
            )
        return obj

    return validator_


# NOTE: Keep this in mind when updating the recommended version:
#  * For all constants below, update platformio.ini (in this repo)
ARDUINO_VERSIONS = {
    "dev": (cv.Version(0, 4, 0), "https://github.com/kuba2k2/libretuya.git"),
    "latest": (cv.Version(0, 4, 0), None),
    "recommended": (cv.Version(0, 4, 0), None),
}


def _check_framework_version(value):
    value = value.copy()

    if value[CONF_VERSION] in ARDUINO_VERSIONS:
        if CONF_SOURCE in value:
            raise cv.Invalid(
                "Framework version needs to be explicitly specified when custom source is used."
            )

        version, source = ARDUINO_VERSIONS[value[CONF_VERSION]]
    else:
        version = cv.Version.parse(cv.version_number(value[CONF_VERSION]))
        source = value.get(CONF_SOURCE, None)

    value[CONF_VERSION] = str(version)
    value[CONF_SOURCE] = source or f"~{version.major}.{version.minor}.{version.patch}"

    return value


FRAMEWORK_ARDUINO = "arduino"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_BOARD): cv.string_strict,
            cv.Optional(CONF_VARIANT): cv.one_of(*VARIANTS, upper=True),
            cv.Optional(CONF_FRAMEWORK, default={}): cv.typed_schema(
                {
                    FRAMEWORK_ARDUINO: cv.All(
                        cv.Schema(
                            {
                                cv.Optional(
                                    CONF_VERSION, default="recommended"
                                ): cv.string_strict,
                                cv.Optional(CONF_SOURCE): cv.string_strict,
                            }
                        ),
                        _check_framework_version,
                    ),
                },
                lower=True,
                space="-",
                default_type=FRAMEWORK_ARDUINO,
            ),
        }
    ),
    _set_variant,
    _set_core_data,
)


async def to_code(config):
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag(f"-DUSE_LIBRETUYA")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "LibreTuya")

    # setup LT logger to work nicely with ESPHome logger
    cg.add_build_flag("-DLT_LOGGER_CALLER=0")
    cg.add_build_flag("-DLT_LOGGER_TASK=0")
    cg.add_build_flag("-DLT_LOGGER_COLOR=1")

    # add monitor options
    cg.add_platformio_option("monitor_speed", "115200")
    cg.add_platformio_option("monitor_filters", "rtl_hard_fault_decoder")

    # add debugging options
    cg.add_platformio_option("debug_tool", "custom")
    cg.add_platformio_option("debug_port", "192.168.0.33:3333")
    cg.add_platformio_option("debug_server", "")
    cg.add_platformio_option(
        "debug_init_cmds",
        [
            "target extended-remote $DEBUG_PORT",
            "$INIT_BREAK",
            "$LOAD_CMDS",
            "monitor init",
        ],
    )

    # disable library compatibility checks
    cg.add_platformio_option("lib_ldf_mode", "off")
    # include <Arduino.h> in every file
    cg.add_platformio_option("build_src_flags", "-include Arduino.h")

    conf = config[CONF_FRAMEWORK]

    # if platform version is a valid version constraint, prefix the default package
    cv.platformio_version_constraint(conf[CONF_VERSION])
    cg.add_platformio_option("platform", f"libretuya @ {conf[CONF_VERSION]}")

    # cg.add_platformio_option("extra_scripts", ["post:post_build.py"])

    # force using arduino framework
    cg.add_platformio_option("framework", "arduino")
    cg.add_build_flag("-DUSE_ARDUINO")

    # dummy version code
    cg.add_define(
        "USE_ARDUINO_VERSION_CODE", cg.RawExpression(f"VERSION_CODE(0, 0, 0)")
    )


# Called by writer.py
def copy_files():
    # if CORE.using_arduino:
    #     write_file_if_changed(
    #         CORE.relative_build_path("partitions.csv"),
    #         ARDUINO_PARTITIONS_CSV,
    #     )
    # if CORE.using_esp_idf:
    #     _write_sdkconfig()
    #     write_file_if_changed(
    #         CORE.relative_build_path("partitions.csv"),
    #         IDF_PARTITIONS_CSV,
    #     )
    #     # IDF build scripts look for version string to put in the build.
    #     # However, if the build path does not have an initialized git repo,
    #     # and no version.txt file exists, the CMake script fails for some setups.
    #     # Fix by manually pasting a version.txt file, containing the ESPHome version
    #     write_file_if_changed(
    #         CORE.relative_build_path("version.txt"),
    #         __version__,
    #     )

    dir = os.path.dirname(__file__)
    post_build_file = os.path.join(dir, "post_build.py.script")
    copy_file_if_changed(
        post_build_file,
        CORE.relative_build_path("post_build.py"),
    )
