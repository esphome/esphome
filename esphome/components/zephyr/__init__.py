import os
from typing import Union

import esphome.codegen as cg
from esphome.const import CONF_BOARD, KEY_NAME
from esphome.core import CORE
from esphome.helpers import copy_file_if_changed, write_file_if_changed

from .const import (
    BOOTLOADER_MCUBOOT,
    KEY_BOOTLOADER,
    KEY_EXTRA_BUILD_FILES,
    KEY_OVERLAY,
    KEY_PATH,
    KEY_PRJ_CONF,
    KEY_ZEPHYR,
    zephyr_ns,
)

CODEOWNERS = ["@tomaszduda23"]
AUTO_LOAD = ["preferences"]
KEY_BOARD = "board"


def zephyr_set_core_data(config):
    CORE.data[KEY_ZEPHYR] = {}
    CORE.data[KEY_ZEPHYR][KEY_BOARD] = config[CONF_BOARD]
    CORE.data[KEY_ZEPHYR][KEY_PRJ_CONF] = {}
    CORE.data[KEY_ZEPHYR][KEY_OVERLAY] = ""
    CORE.data[KEY_ZEPHYR][KEY_BOOTLOADER] = config[KEY_BOOTLOADER]
    CORE.data[KEY_ZEPHYR][KEY_EXTRA_BUILD_FILES] = {}
    return config


PrjConfValueType = Union[bool, str, int]


def zephyr_add_prj_conf(name: str, value: PrjConfValueType, required: bool = True):
    """Set an zephyr prj conf value."""
    if not name.startswith("CONFIG_"):
        name = "CONFIG_" + name
    if name in CORE.data[KEY_ZEPHYR][KEY_PRJ_CONF]:
        old_value = CORE.data[KEY_ZEPHYR][KEY_PRJ_CONF][name]
        if old_value[0] != value and old_value[1]:
            raise ValueError(
                f"{name} already set with value '{old_value[0]}', cannot set again to '{value}'"
            )
        if required:
            CORE.data[KEY_ZEPHYR][KEY_PRJ_CONF][name] = (value, required)
    else:
        CORE.data[KEY_ZEPHYR][KEY_PRJ_CONF][name] = (value, required)


def zephyr_add_overlay(content):
    CORE.data[KEY_ZEPHYR][KEY_OVERLAY] += content


def add_extra_build_file(filename: str, path: str) -> bool:
    """Add an extra build file to the project."""
    if filename not in CORE.data[KEY_ZEPHYR][KEY_EXTRA_BUILD_FILES]:
        CORE.data[KEY_ZEPHYR][KEY_EXTRA_BUILD_FILES][filename] = {
            KEY_NAME: filename,
            KEY_PATH: path,
        }
        return True
    return False


def add_extra_script(stage: str, filename: str, path: str):
    """Add an extra script to the project."""
    key = f"{stage}:{filename}"
    if add_extra_build_file(filename, path):
        cg.add_platformio_option("extra_scripts", [key])


def zephyr_to_code(conf):
    cg.add(zephyr_ns.setup_preferences())
    cg.add_build_flag("-DUSE_ZEPHYR")
    # build is done by west so bypass board checking in platformio
    cg.add_platformio_option("boards_dir", CORE.relative_build_path("boards"))

    # c++ support
    zephyr_add_prj_conf("NEWLIB_LIBC", True)
    zephyr_add_prj_conf("CONFIG_FPU", True)
    zephyr_add_prj_conf("NEWLIB_LIBC_FLOAT_PRINTF", True)
    zephyr_add_prj_conf("CPLUSPLUS", True)
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

    add_extra_script(
        "pre",
        "pre_build.py",
        os.path.join(os.path.dirname(__file__), "pre_build.py.script"),
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


def copy_files():
    want_opts = CORE.data[KEY_ZEPHYR][KEY_PRJ_CONF]

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
        CORE.data[KEY_ZEPHYR][KEY_OVERLAY],
    )

    if CORE.data[KEY_ZEPHYR][KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT:
        fake_board_manifest = """
{
"frameworks": [
    "zephyr"
],
"name": "esphome nrf52",
"upload": {
    "maximum_ram_size": 248832,
    "maximum_size": 815104
},
"url": "https://esphome.io/",
"vendor": "esphome"
}
"""
        write_file_if_changed(
            CORE.relative_build_path(f"boards/{CORE.data[KEY_ZEPHYR][KEY_BOARD]}.json"),
            fake_board_manifest,
        )

    for _, file in CORE.data[KEY_ZEPHYR][KEY_EXTRA_BUILD_FILES].items():
        copy_file_if_changed(
            file[KEY_PATH],
            CORE.relative_build_path(file[KEY_NAME]),
        )
