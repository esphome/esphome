import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_COMPONENT_ID,
    CONF_FAMILY,
    CONF_FRAMEWORK,
    CONF_ID,
    CONF_NAME,
    CONF_PROJECT,
    CONF_SOURCE,
    CONF_VERSION,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    __version__,
)
from esphome.core import CORE

from . import gpio  # noqa
from .const import (
    CONF_GPIO_RECOVER,
    CONF_LOGLEVEL,
    CONF_LT_CONFIG,
    CONF_SDK_SILENT,
    CONF_SDK_SILENT_ALL,
    FAMILIES,
    FAMILY_COMPONENT,
    FAMILY_FRIENDLY,
    KEY_BOARD,
    KEY_COMPONENT,
    KEY_COMPONENT_DATA,
    KEY_FAMILY,
    KEY_LIBRETINY,
    LT_LOGLEVELS,
    LibreTinyComponent,
    LTComponent,
)

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@kuba2k2"]
AUTO_LOAD = []


def _detect_variant(value):
    if KEY_LIBRETINY not in CORE.data:
        raise cv.Invalid("Family component didn't populate core data properly!")
    component: LibreTinyComponent = CORE.data[KEY_LIBRETINY][KEY_COMPONENT_DATA]
    board = value[CONF_BOARD]
    # read board-default family if not specified
    if CONF_FAMILY not in value:
        if board not in component.boards:
            raise cv.Invalid(
                "This board is unknown, please set the family manually. "
                "Also, make sure the chosen chip component is correct.",
                path=[CONF_BOARD],
            )
        value = value.copy()
        value[CONF_FAMILY] = component.boards[board][KEY_FAMILY]
    # read component name matching this family
    value[CONF_COMPONENT_ID] = FAMILY_COMPONENT[value[CONF_FAMILY]]
    # make sure the chosen component matches the family
    if value[CONF_COMPONENT_ID] != component.name:
        raise cv.Invalid(
            f"The chosen family doesn't belong to '{component.name}' component. The correct component is '{value[CONF_COMPONENT_ID]}'",
            path=["variant"],
        )
    # warn anyway if the board wasn't found
    if board not in component.boards:
        _LOGGER.warning(
            "This board is unknown. Make sure the chosen chip component is correct.",
        )
    return value


def _update_core_data(config):
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = config[CONF_COMPONENT_ID]
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "arduino"
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version.parse(
        config[CONF_FRAMEWORK][CONF_VERSION]
    )
    CORE.data[KEY_LIBRETINY][KEY_BOARD] = config[CONF_BOARD]
    CORE.data[KEY_LIBRETINY][KEY_COMPONENT] = config[CONF_COMPONENT_ID]
    CORE.data[KEY_LIBRETINY][KEY_FAMILY] = config[CONF_FAMILY]
    return config


def get_libretiny_component(core_obj=None):
    return (core_obj or CORE).data[KEY_LIBRETINY][KEY_COMPONENT]


def get_libretiny_family(core_obj=None):
    return (core_obj or CORE).data[KEY_LIBRETINY][KEY_FAMILY]


def only_on_family(*, supported=None, unsupported=None):
    """Config validator for features only available on some LibreTiny families."""
    if supported is not None and not isinstance(supported, list):
        supported = [supported]
    if unsupported is not None and not isinstance(unsupported, list):
        unsupported = [unsupported]

    def validator_(obj):
        family = get_libretiny_family()
        if supported is not None and family not in supported:
            raise cv.Invalid(
                f"This feature is only available on {', '.join(supported)}"
            )
        if unsupported is not None and family in unsupported:
            raise cv.Invalid(
                f"This feature is not available on {', '.join(unsupported)}"
            )
        return obj

    return validator_


def _notify_old_style(config):
    if config:
        raise cv.Invalid(
            "The LibreTiny component is now split between supported chip families.\n"
            "Migrate your config file to include a chip-based configuration, "
            "instead of the 'libretiny:' block.\n"
            "For example 'bk72xx:' or 'rtl87xx:'."
        )
    return config


# NOTE: Keep this in mind when updating the recommended version:
#  * For all constants below, update platformio.ini (in this repo)
ARDUINO_VERSIONS = {
    "dev": (cv.Version(0, 0, 0), "https://github.com/kuba2k2/libretiny.git"),
    "latest": (cv.Version(0, 0, 0), None),
    "recommended": (cv.Version(1, 2, 0), None),
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
    value[CONF_SOURCE] = source

    return value


FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_VERSION, default="recommended"): cv.string_strict,
            cv.Optional(CONF_SOURCE): cv.string_strict,
        }
    ),
    _check_framework_version,
)

CONFIG_SCHEMA = cv.All(_notify_old_style)

BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LTComponent),
        cv.Required(CONF_BOARD): cv.string_strict,
        cv.Optional(CONF_FAMILY): cv.one_of(*FAMILIES, upper=True),
        cv.Optional(CONF_FRAMEWORK, default={}): FRAMEWORK_SCHEMA,
        cv.Optional(CONF_LT_CONFIG, default={}): {
            cv.string_strict: cv.string,
        },
        cv.Optional(CONF_LOGLEVEL, default="warn"): cv.one_of(
            *LT_LOGLEVELS, upper=True
        ),
        cv.Optional(CONF_SDK_SILENT, default=True): cv.boolean,
        cv.Optional(CONF_SDK_SILENT_ALL, default=True): cv.boolean,
        cv.Optional(CONF_GPIO_RECOVER, default=True): cv.boolean,
    },
)

BASE_SCHEMA.add_extra(_detect_variant)
BASE_SCHEMA.add_extra(_update_core_data)


# pylint: disable=use-dict-literal
async def component_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    # setup board config
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_LIBRETINY")
    cg.add_build_flag(f"-DUSE_{config[CONF_COMPONENT_ID]}")
    cg.add_build_flag(f"-DUSE_LIBRETINY_VARIANT_{config[CONF_FAMILY]}")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", FAMILY_FRIENDLY[config[CONF_FAMILY]])

    # setup LT logger to work nicely with ESPHome logger
    lt_config = dict(
        LT_LOGLEVEL="LT_LEVEL_" + config[CONF_LOGLEVEL],
        LT_LOGGER_CALLER=0,
        LT_LOGGER_TASK=0,
        LT_LOGGER_COLOR=1,
        LT_DEBUG_ALL=1,
        LT_UART_SILENT_ENABLED=int(config[CONF_SDK_SILENT]),
        LT_UART_SILENT_ALL=int(config[CONF_SDK_SILENT_ALL]),
        LT_USE_TIME=1,
    )
    lt_config.update(config[CONF_LT_CONFIG])

    # force using arduino framework
    cg.add_platformio_option("framework", "arduino")
    cg.add_build_flag("-DUSE_ARDUINO")

    # disable library compatibility checks
    cg.add_platformio_option("lib_ldf_mode", "off")
    # include <Arduino.h> in every file
    cg.add_platformio_option("build_src_flags", "-include Arduino.h")

    # if platform version is a valid version constraint, prefix the default package
    framework = config[CONF_FRAMEWORK]
    cv.platformio_version_constraint(framework[CONF_VERSION])
    if str(framework[CONF_VERSION]) != "0.0.0":
        cg.add_platformio_option("platform", f"libretiny @ {framework[CONF_VERSION]}")
    elif framework[CONF_SOURCE]:
        cg.add_platformio_option("platform", framework[CONF_SOURCE])
    else:
        cg.add_platformio_option("platform", "libretiny")

    # add LT configuration options
    for name, value in sorted(lt_config.items()):
        cg.add_build_flag(f"-D{name}={value}")

    # add ESPHome LT-related options
    cg.add_define("LT_GPIO_RECOVER", int(config[CONF_GPIO_RECOVER]))

    # dummy version code
    cg.add_define("USE_ARDUINO_VERSION_CODE", cg.RawExpression("VERSION_CODE(0, 0, 0)"))

    # decrease web server stack size (16k words -> 4k words)
    cg.add_build_flag("-DCONFIG_ASYNC_TCP_STACK_SIZE=4096")

    # custom output firmware name and version
    if CONF_PROJECT in config:
        cg.add_platformio_option(
            "custom_fw_name", "esphome." + config[CONF_PROJECT][CONF_NAME]
        )
        cg.add_platformio_option(
            "custom_fw_version", config[CONF_PROJECT][CONF_VERSION]
        )
    else:
        cg.add_platformio_option("custom_fw_name", "esphome")
        cg.add_platformio_option("custom_fw_version", __version__)

    await cg.register_component(var, config)
