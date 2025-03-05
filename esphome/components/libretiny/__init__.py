import json
import logging
from os.path import dirname, isfile, join

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_COMPONENT_ID,
    CONF_DEBUG,
    CONF_FAMILY,
    CONF_FRAMEWORK,
    CONF_ID,
    CONF_NAME,
    CONF_OPTIONS,
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
    CONF_SDK_SILENT,
    CONF_UART_PORT,
    FAMILIES,
    FAMILY_COMPONENT,
    FAMILY_FRIENDLY,
    KEY_BOARD,
    KEY_COMPONENT,
    KEY_COMPONENT_DATA,
    KEY_FAMILY,
    KEY_LIBRETINY,
    LT_DEBUG_MODULES,
    LT_LOGLEVELS,
    LibreTinyComponent,
    LTComponent,
)

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@kuba2k2"]
AUTO_LOAD = ["preferences"]
IS_TARGET_PLATFORM = True


def _detect_variant(value):
    if KEY_LIBRETINY not in CORE.data:
        raise cv.Invalid("Family component didn't populate core data properly!")
    component: LibreTinyComponent = CORE.data[KEY_LIBRETINY][KEY_COMPONENT_DATA]
    board = value[CONF_BOARD]
    # read board-default family if not specified
    if board not in component.boards:
        if CONF_FAMILY not in value:
            raise cv.Invalid(
                "This board is unknown, if you are sure you want to compile with this board selection, "
                f"override with option '{CONF_FAMILY}'",
                path=[CONF_BOARD],
            )
        _LOGGER.warning(
            "This board is unknown. Make sure the chosen chip component is correct.",
        )
    else:
        family = component.boards[board][KEY_FAMILY]
        if CONF_FAMILY in value and family != value[CONF_FAMILY]:
            raise cv.Invalid(
                f"Option '{CONF_FAMILY}' does not match selected board.",
                path=[CONF_FAMILY],
            )
        value = value.copy()
        value[CONF_FAMILY] = family
    # read component name matching this family
    value[CONF_COMPONENT_ID] = FAMILY_COMPONENT[value[CONF_FAMILY]]
    # make sure the chosen component matches the family
    if value[CONF_COMPONENT_ID] != component.name:
        raise cv.Invalid(
            f"The chosen family doesn't belong to '{component.name}' component. The correct component is '{value[CONF_COMPONENT_ID]}'",
            path=[CONF_FAMILY],
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


def get_download_types(storage_json=None):
    types = [
        {
            "title": "UF2 package (recommended)",
            "description": "For flashing via web_server OTA or with ltchiptool (UART)",
            "file": "firmware.uf2",
            "download": f"{storage_json.name}.uf2",
        },
    ]

    build_dir = dirname(storage_json.firmware_bin_path)
    outputs = join(build_dir, "firmware.json")
    if not isfile(outputs):
        return types
    with open(outputs, encoding="utf-8") as f:
        outputs = json.load(f)
    for output in outputs:
        if not output["public"]:
            continue
        suffix = output["filename"].partition(".")[2]
        suffix = f"-{suffix}" if "." in suffix else f".{suffix}"
        types.append(
            {
                "title": output["title"],
                "description": output["description"],
                "file": output["filename"],
                "download": storage_json.name + suffix,
            }
        )
    return types


def _notify_old_style(config):
    if config:
        raise cv.Invalid(
            "The LibreTiny component is now split between supported chip families.\n"
            "Migrate your config file to include a chip-based configuration, "
            "instead of the 'libretiny:' block.\n"
            "For example 'bk72xx:' or 'rtl87xx:'."
        )
    return config


# The dev and latest branches will be at *least* this version, which is what matters.
ARDUINO_VERSIONS = {
    "dev": (cv.Version(1, 7, 0), "https://github.com/libretiny-eu/libretiny.git"),
    "latest": (cv.Version(1, 7, 0), "libretiny"),
    "recommended": (cv.Version(1, 7, 0), None),
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


def _check_debug_order(value):
    debug = value[CONF_DEBUG]
    if "NONE" in debug and "NONE" in debug[1:]:
        raise cv.Invalid(
            "'none' has to be specified before other modules, and only once",
            path=[CONF_DEBUG],
        )
    return value


FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_VERSION, default="recommended"): cv.string_strict,
            cv.Optional(CONF_SOURCE): cv.string_strict,
            cv.Optional(CONF_LOGLEVEL, default="warn"): (
                cv.one_of(*LT_LOGLEVELS, upper=True)
            ),
            cv.Optional(CONF_DEBUG, default=[]): cv.ensure_list(
                cv.one_of("NONE", *LT_DEBUG_MODULES, upper=True)
            ),
            cv.Optional(CONF_SDK_SILENT, default="all"): (
                cv.one_of("all", "auto", "none", lower=True)
            ),
            cv.Optional(CONF_UART_PORT): cv.one_of(0, 1, 2, int=True),
            cv.Optional(CONF_GPIO_RECOVER, default=True): cv.boolean,
            cv.Optional(CONF_OPTIONS, default={}): {
                cv.string_strict: cv.string,
            },
        }
    ),
    _check_framework_version,
    _check_debug_order,
)

CONFIG_SCHEMA = cv.All(_notify_old_style)

BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LTComponent),
        cv.Required(CONF_BOARD): cv.string_strict,
        cv.Optional(CONF_FAMILY): cv.one_of(*FAMILIES, upper=True),
        cv.Optional(CONF_FRAMEWORK, default={}): FRAMEWORK_SCHEMA,
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
    cg.add_build_flag(f"-DUSE_{config[CONF_COMPONENT_ID].upper()}")
    cg.add_build_flag(f"-DUSE_LIBRETINY_VARIANT_{config[CONF_FAMILY]}")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", FAMILY_FRIENDLY[config[CONF_FAMILY]])

    # force using arduino framework
    cg.add_platformio_option("framework", "arduino")
    cg.add_build_flag("-DUSE_ARDUINO")

    # disable library compatibility checks
    cg.add_platformio_option("lib_ldf_mode", "off")
    # include <Arduino.h> in every file
    cg.add_platformio_option("build_src_flags", "-include Arduino.h")
    # dummy version code
    cg.add_define("USE_ARDUINO_VERSION_CODE", cg.RawExpression("VERSION_CODE(0, 0, 0)"))
    # decrease web server stack size (16k words -> 4k words)
    cg.add_build_flag("-DCONFIG_ASYNC_TCP_STACK_SIZE=4096")

    # build framework version
    # if platform version is a valid version constraint, prefix the default package
    framework = config[CONF_FRAMEWORK]
    cv.platformio_version_constraint(framework[CONF_VERSION])
    if framework[CONF_SOURCE]:
        cg.add_platformio_option("platform", framework[CONF_SOURCE])
    elif str(framework[CONF_VERSION]) != "0.0.0":
        cg.add_platformio_option("platform", f"libretiny @ {framework[CONF_VERSION]}")
    else:
        cg.add_platformio_option("platform", "libretiny")

    # apply LibreTiny options from framework: block
    # setup LT logger to work nicely with ESPHome logger
    lt_options = dict(
        LT_LOGLEVEL="LT_LEVEL_" + framework[CONF_LOGLEVEL],
        LT_LOGGER_CALLER=0,
        LT_LOGGER_TASK=0,
        LT_LOGGER_COLOR=1,
        LT_USE_TIME=1,
    )
    # enable/disable per-module debugging
    for module in framework[CONF_DEBUG]:
        if module == "NONE":
            # disable all modules
            for module in LT_DEBUG_MODULES:
                lt_options[f"LT_DEBUG_{module}"] = 0
        else:
            # enable one module
            lt_options[f"LT_DEBUG_{module}"] = 1
    # set SDK silencing mode
    if framework[CONF_SDK_SILENT] == "all":
        lt_options["LT_UART_SILENT_ENABLED"] = 1
        lt_options["LT_UART_SILENT_ALL"] = 1
    elif framework[CONF_SDK_SILENT] == "auto":
        lt_options["LT_UART_SILENT_ENABLED"] = 1
        lt_options["LT_UART_SILENT_ALL"] = 0
    else:
        lt_options["LT_UART_SILENT_ENABLED"] = 0
        lt_options["LT_UART_SILENT_ALL"] = 0
    # set default UART port
    if (uart_port := framework.get(CONF_UART_PORT, None)) is not None:
        lt_options["LT_UART_DEFAULT_PORT"] = uart_port
    # add custom options
    lt_options.update(framework[CONF_OPTIONS])

    # apply ESPHome options from framework: block
    cg.add_define("LT_GPIO_RECOVER", int(framework[CONF_GPIO_RECOVER]))

    # build PlatformIO compiler flags
    for name, value in sorted(lt_options.items()):
        cg.add_build_flag(f"-D{name}={value}")

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
