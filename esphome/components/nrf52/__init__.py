import os
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_NRF52,
    CONF_TYPE,
    CONF_FRAMEWORK,
    CONF_VARIANT,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.helpers import (
    write_file_if_changed,
    copy_file_if_changed,
)
from typing import Union

# force import gpio to register pin schema
from .gpio import nrf52_pin_to_code  # noqa

KEY_NRF52 = "nrf52"


def set_core_data(config):
    CORE.data[KEY_NRF52] = {}
    CORE.data[KEY_NRF52][KEY_PRJ_CONF_OPTIONS] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_NRF52
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = config[CONF_FRAMEWORK][CONF_TYPE]
    return config


# https://github.com/platformio/platform-nordicnrf52/releases
NORDICNRF52_PLATFORM_VERSION = cv.Version(10, 3, 0)


def _platform_check_versions(value):
    value = value.copy()
    value[CONF_PLATFORM_VERSION] = value.get(
        CONF_PLATFORM_VERSION,
        _parse_platform_version(str(NORDICNRF52_PLATFORM_VERSION)),
    )
    return value


def _parse_platform_version(value):
    try:
        # if platform version is a valid version constraint, prefix the default package
        cv.platformio_version_constraint(value)
        return f"platformio/nordicnrf52@{value}"
    except cv.Invalid:
        return value


CONF_PLATFORM_VERSION = "platform_version"

PLATFORM_FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_PLATFORM_VERSION): _parse_platform_version,
        }
    ),
    _platform_check_versions,
)

ZEPHYR_VARIANT_GENERIC = "generic"
ZEPHYR_VARIANT_NRF_SDK = "nrf-sdk"
ZEPHYR_VARIANTS = [
    ZEPHYR_VARIANT_GENERIC,
    ZEPHYR_VARIANT_NRF_SDK,
]

FRAMEWORK_VARIANTS = [
    "zephyr",
    "arduino",
]

FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_TYPE): cv.one_of(*FRAMEWORK_VARIANTS, lower=True),
            cv.Optional(CONF_VARIANT): cv.one_of(*ZEPHYR_VARIANTS, lower=True),
        }
    ),
    _platform_check_versions,
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_BOARD): cv.string_strict,
            cv.Optional(CONF_FRAMEWORK, default={}): FRAMEWORK_SCHEMA,
        }
    ),
    set_core_data,
)

nrf52_ns = cg.esphome_ns.namespace("nrf52")


PrjConfValueType = Union[bool, str, int]
KEY_PRJ_CONF_OPTIONS = "prj_conf_options"


def add_zephyr_prj_conf_option(name: str, value: PrjConfValueType):
    """Set an zephyr prj conf value."""
    if not CORE.using_zephyr:
        raise ValueError("Not an zephyr project")
    if name in CORE.data[KEY_NRF52][KEY_PRJ_CONF_OPTIONS]:
        old_value = CORE.data[KEY_NRF52][KEY_PRJ_CONF_OPTIONS][name]
        if old_value != value:
            raise ValueError(
                f"{name} alread set with value {old_value}, new value {value}"
            )
    CORE.data[KEY_NRF52][KEY_PRJ_CONF_OPTIONS][name] = value


@coroutine_with_priority(1000)
async def to_code(config):
    cg.add(nrf52_ns.setup_preferences())
    if config[CONF_BOARD] == "nrf52840":
        if CORE.using_zephyr:
            # this board works with https://github.com/adafruit/Adafruit_nRF52_Bootloader
            config[CONF_BOARD] = "adafruit_itsybitsy_nrf52840"
        elif CORE.using_arduino:
            # it has most generic GPIO mapping
            # TODO does it matter if custom board is defined?
            config[CONF_BOARD] = "nrf52840_dk_adafruit"
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_NRF52")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "NRF52")
    conf = config[CONF_FRAMEWORK]
    cg.add_platformio_option(CONF_FRAMEWORK, conf[CONF_TYPE])
    cg.add_platformio_option("platform", conf[CONF_PLATFORM_VERSION])

    # make sure that firmware.zip is created
    # for Adafruit_nRF52_Bootloader
    # TODO add bootloader type to config
    cg.add_platformio_option("board_upload.protocol", "nrfutil")
    cg.add_platformio_option("board_upload.use_1200bps_touch", "true")
    cg.add_platformio_option("board_upload.require_upload_port", "true")
    cg.add_platformio_option("board_upload.wait_for_upload_port", "true")
    cg.add_platformio_option("extra_scripts", [f"pre:build_{conf[CONF_TYPE]}.py"])
    if CORE.using_arduino:
        cg.add_build_flag("-DUSE_ARDUINO")
        # watchdog
        cg.add_build_flag("-DNRFX_WDT_ENABLED=1")
        cg.add_build_flag("-DNRFX_WDT0_ENABLED=1")
        cg.add_build_flag("-DNRFX_WDT_CONFIG_NO_IRQ=1")
        # prevent setting up GPIO PINs
        cg.add_platformio_option("board_build.variant", "nrf52840")
        cg.add_platformio_option(
            "board_build.variants_dir", os.path.dirname(os.path.realpath(__file__))
        )
        cg.add_library("https://github.com/NordicSemiconductor/nrfx#v2.1.0", None, None)
    elif CORE.using_zephyr:
        cg.add_build_flag("-DUSE_ZEPHYR")
        if conf[CONF_VARIANT] == ZEPHYR_VARIANT_GENERIC:
            cg.add_platformio_option(
                "platform_packages",
                [
                    "platformio/framework-zephyr@^2.30500.231204",
                    # "platformio/toolchain-gccarmnoneeabi@^1.120301.0"
                ],
            )
        elif conf[CONF_VARIANT] == ZEPHYR_VARIANT_NRF_SDK:
            cg.add_platformio_option(
                "platform_packages",
                [
                    "platformio/framework-zephyr@https://github.com/tomaszduda23/framework-sdk-nrf",
                    "platformio/toolchain-gccarmnoneeabi@https://github.com/tomaszduda23/toolchain-sdk-ng",
                ],
            )
        else:
            raise NotImplementedError
        # c++ support
        add_zephyr_prj_conf_option("CONFIG_NEWLIB_LIBC", False)
        add_zephyr_prj_conf_option("CONFIG_NEWLIB_LIBC_NANO", True)
        add_zephyr_prj_conf_option("CONFIG_NEWLIB_LIBC_FLOAT_PRINTF", True)
        add_zephyr_prj_conf_option("CONFIG_CPLUSPLUS", True)
        add_zephyr_prj_conf_option("CONFIG_LIB_CPLUSPLUS", True)
        # watchdog
        add_zephyr_prj_conf_option("CONFIG_WATCHDOG", True)
        add_zephyr_prj_conf_option("CONFIG_WDT_DISABLE_AT_BOOT", False)
        # TODO debug only
        add_zephyr_prj_conf_option("CONFIG_DEBUG_THREAD_INFO", True)

    else:
        raise NotImplementedError
    # add_zephyr_prj_conf_option("CONFIG_USE_SEGGER_RTT", True)
    # add_zephyr_prj_conf_option("CONFIG_RTT_CONSOLE", True)
    # add_zephyr_prj_conf_option("CONFIG_UART_CONSOLE", False)


def _format_prj_conf_val(value: PrjConfValueType) -> str:
    if isinstance(value, bool):
        return "y" if value else "n"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, str):
        return f'"{value}"'
    raise ValueError


overlay = """
"""


# Called by writer.py
def copy_files():
    if CORE.using_zephyr:
        want_opts = CORE.data[KEY_NRF52][KEY_PRJ_CONF_OPTIONS]
        contents = (
            "\n".join(
                f"{name}={_format_prj_conf_val(value)}"
                for name, value in sorted(want_opts.items())
            )
            + "\n"
        )

        write_file_if_changed(CORE.relative_build_path("zephyr/prj.conf"), contents)
        write_file_if_changed(CORE.relative_build_path("zephyr/app.overlay"), overlay)

    dir = os.path.dirname(__file__)
    build_zephyr_file = os.path.join(
        dir, f"build_{CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK]}.py.script"
    )
    copy_file_if_changed(
        build_zephyr_file,
        CORE.relative_build_path(
            f"build_{CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK]}.py"
        ),
    )
