from pathlib import Path
import textwrap
from typing import TypedDict

import esphome.codegen as cg
from esphome.const import CONF_BOARD
from esphome.core import CORE
from esphome.helpers import copy_file_if_changed, write_file_if_changed

from .const import (
    BOOTLOADER_MCUBOOT,
    KEY_BOARD,
    KEY_BOOTLOADER,
    KEY_EXTRA_BUILD_FILES,
    KEY_OVERLAY,
    KEY_PM_STATIC,
    KEY_PRJ_CONF,
    KEY_USER,
    KEY_ZEPHYR,
    zephyr_ns,
)

CODEOWNERS = ["@tomaszduda23"]
AUTO_LOAD = ["preferences"]

PrjConfValueType = bool | str | int


class Section:
    def __init__(self, name, address, size, region):
        self.name = name
        self.address = address
        self.size = size
        self.region = region
        self.end_address = self.address + self.size

    def __str__(self):
        return (
            f"{self.name}:\n"
            f"  address: 0x{self.address:X}\n"
            f"  end_address: 0x{self.end_address:X}\n"
            f"  region: {self.region}\n"
            f"  size: 0x{self.size:X}"
        )


class ZephyrData(TypedDict):
    board: str
    bootloader: str
    prj_conf: dict[str, tuple[PrjConfValueType, bool]]
    overlay: str
    extra_build_files: dict[str, Path]
    pm_static: list[Section]
    user: dict[str, list[str]]


def zephyr_set_core_data(config):
    CORE.data[KEY_ZEPHYR] = ZephyrData(
        board=config[CONF_BOARD],
        bootloader=config[KEY_BOOTLOADER],
        prj_conf={},
        overlay="",
        extra_build_files={},
        pm_static=[],
        user={},
    )
    return config


def zephyr_data() -> ZephyrData:
    return CORE.data[KEY_ZEPHYR]


def zephyr_add_prj_conf(
    name: str, value: PrjConfValueType, required: bool = True
) -> None:
    """Set an zephyr prj conf value."""
    if not name.startswith("CONFIG_"):
        name = "CONFIG_" + name
    prj_conf = zephyr_data()[KEY_PRJ_CONF]
    if name not in prj_conf:
        prj_conf[name] = (value, required)
        return
    old_value, old_required = prj_conf[name]
    if old_value != value and old_required:
        raise ValueError(
            f"{name} already set with value '{old_value}', cannot set again to '{value}'"
        )
    if required:
        prj_conf[name] = (value, required)


def zephyr_add_overlay(content):
    zephyr_data()[KEY_OVERLAY] += textwrap.dedent(content)


def add_extra_build_file(filename: str, path: Path) -> bool:
    """Add an extra build file to the project."""
    extra_build_files = zephyr_data()[KEY_EXTRA_BUILD_FILES]
    if filename not in extra_build_files:
        extra_build_files[filename] = path
        return True
    return False


def add_extra_script(stage: str, filename: str, path: Path) -> None:
    """Add an extra script to the project."""
    key = f"{stage}:{filename}"
    if add_extra_build_file(filename, path):
        cg.add_platformio_option("extra_scripts", [key])


def zephyr_to_code(config):
    cg.add(zephyr_ns.setup_preferences())
    cg.add_build_flag("-DUSE_ZEPHYR")
    cg.set_cpp_standard("gnu++20")
    # build is done by west so bypass board checking in platformio
    cg.add_platformio_option("boards_dir", CORE.relative_build_path("boards"))

    # c++ support
    zephyr_add_prj_conf("NEWLIB_LIBC", True)
    zephyr_add_prj_conf("CONFIG_FPU", True)
    zephyr_add_prj_conf("NEWLIB_LIBC_FLOAT_PRINTF", True)
    zephyr_add_prj_conf("CPLUSPLUS", True)
    zephyr_add_prj_conf("CONFIG_STD_CPP20", True)
    zephyr_add_prj_conf("LIB_CPLUSPLUS", True)
    # preferences
    zephyr_add_prj_conf("SETTINGS", True)
    zephyr_add_prj_conf("NVS", True)
    zephyr_add_prj_conf("FLASH_MAP", True)
    zephyr_add_prj_conf("CONFIG_FLASH", True)
    # watchdog
    zephyr_add_prj_conf("WATCHDOG", True)
    zephyr_add_prj_conf("WDT_DISABLE_AT_BOOT", False)
    # disable console
    zephyr_add_prj_conf("UART_CONSOLE", False)
    zephyr_add_prj_conf("CONSOLE", False, False)
    # use NFC pins as GPIO
    zephyr_add_prj_conf("NFCT_PINS_AS_GPIOS", True)

    # <err> os: ***** USAGE FAULT *****
    # <err> os:   Illegal load of EXC_RETURN into PC
    zephyr_add_prj_conf("MAIN_STACK_SIZE", 2048)

    add_extra_script(
        "pre",
        "pre_build.py",
        Path(__file__).parent / "pre_build.py.script",
    )


def _format_prj_conf_val(value: PrjConfValueType) -> str:
    if isinstance(value, bool):
        return "y" if value else "n"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, str):
        return f'"{value}"'
    raise ValueError


def zephyr_add_cdc_acm(config, id):
    zephyr_add_prj_conf("USB_DEVICE_STACK", True)
    zephyr_add_prj_conf("USB_CDC_ACM", True)
    # prevent device to go to susspend, without this communication stop working in python
    # there should be a way to solve it
    zephyr_add_prj_conf("USB_DEVICE_REMOTE_WAKEUP", False)
    # prevent logging when buffer is full
    zephyr_add_prj_conf("USB_CDC_ACM_LOG_LEVEL_WRN", True)
    zephyr_add_overlay(
        f"""
&zephyr_udc0 {{
    cdc_acm_uart{id}: cdc_acm_uart{id} {{
        compatible = "zephyr,cdc-acm-uart";
    }};
}};
"""
    )


def zephyr_add_pm_static(section: Section):
    CORE.data[KEY_ZEPHYR][KEY_PM_STATIC].extend(section)


def zephyr_add_user(key, value):
    user = zephyr_data()[KEY_USER]
    if key not in user:
        user[key] = []
    user[key] += [value]


def copy_files():
    user = zephyr_data()[KEY_USER]
    if user:
        zephyr_add_overlay(
            f"""
/ {{
    zephyr,user {{
        {[f"{key} = {', '.join(value)};" for key, value in user.items()][0]}
}};
}};"""
        )

    want_opts = zephyr_data()[KEY_PRJ_CONF]

    prj_conf = (
        "\n".join(
            f"{name}={_format_prj_conf_val(value[0])}"
            for name, value in sorted(want_opts.items())
        )
        + "\n"
    )

    write_file_if_changed(CORE.relative_build_path("zephyr/prj.conf"), prj_conf)

    write_file_if_changed(
        CORE.relative_build_path("zephyr/app.overlay"),
        zephyr_data()[KEY_OVERLAY],
    )

    if zephyr_data()[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT or zephyr_data()[
        KEY_BOARD
    ] in ["xiao_ble"]:
        fake_board_manifest = """
{
    "frameworks": [
        "zephyr"
    ],
    "name": "esphome nrf52",
    "upload": {
        "maximum_ram_size": 248832,
        "maximum_size": 815104,
        "speed": 115200
    },
    "url": "https://esphome.io/",
    "vendor": "esphome",
    "build": {
        "bsp": {
            "name": "adafruit"
        },
        "softdevice": {
            "sd_fwid": "0x00B6"
        }
    }
}
"""

        write_file_if_changed(
            CORE.relative_build_path(f"boards/{zephyr_data()[KEY_BOARD]}.json"),
            fake_board_manifest,
        )

    for filename, path in zephyr_data()[KEY_EXTRA_BUILD_FILES].items():
        copy_file_if_changed(
            path,
            CORE.relative_build_path(filename),
        )

    pm_static = "\n".join(str(item) for item in zephyr_data()[KEY_PM_STATIC])
    if pm_static:
        write_file_if_changed(
            CORE.relative_build_path("zephyr/pm_static.yml"), pm_static
        )
