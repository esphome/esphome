import esphome.codegen as cg
from typing import Union
from esphome.core import CORE
from esphome.helpers import (
    write_file_if_changed,
)
from .const import (
    ZEPHYR_VARIANT_GENERIC,
    ZEPHYR_VARIANT_NRF_SDK,
    KEY_ZEPHYR,
    KEY_PRJ_CONF,
    KEY_OVERLAY,
    zephyr_ns,
)
from esphome.const import (
    CONF_VARIANT,
    CONF_BOARD,
)


AUTO_LOAD = ["preferences"]
KEY_BOARD = "board"


def zephyr_set_core_data(config):
    CORE.data[KEY_ZEPHYR] = {}
    CORE.data[KEY_ZEPHYR][KEY_BOARD] = config[CONF_BOARD]
    CORE.data[KEY_ZEPHYR][KEY_PRJ_CONF] = {}
    CORE.data[KEY_ZEPHYR][KEY_OVERLAY] = ""
    return config


PrjConfValueType = Union[bool, str, int]


def zephyr_add_prj_conf(name: str, value: PrjConfValueType):
    """Set an zephyr prj conf value."""
    if not CORE.using_zephyr:
        raise ValueError("Not an zephyr project")
    if not name.startswith("CONFIG_"):
        name = "CONFIG_" + name
    if name in CORE.data[KEY_ZEPHYR][KEY_PRJ_CONF]:
        old_value = CORE.data[KEY_ZEPHYR][KEY_PRJ_CONF][name]
        if old_value != value:
            raise ValueError(
                f"{name} alread set with value {old_value}, new value {value}"
            )
    CORE.data[KEY_ZEPHYR][KEY_PRJ_CONF][name] = value


def zephyr_add_overlay(content):
    if not CORE.using_zephyr:
        raise ValueError("Not an zephyr project")
    CORE.data[KEY_ZEPHYR][KEY_OVERLAY] += content


def zephyr_to_code(conf):
    cg.add(zephyr_ns.setup_preferences())
    cg.add_build_flag("-DUSE_ZEPHYR")
    if conf[CONF_VARIANT] == ZEPHYR_VARIANT_GENERIC:
        cg.add_platformio_option(
            "platform_packages",
            [
                "platformio/framework-zephyr@^2.30500.231204",
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
        # build is done by west so bypass board checking in platformio
        cg.add_platformio_option("boards_dir", CORE.relative_build_path("boards"))
    else:
        raise NotImplementedError
    # c++ support
    zephyr_add_prj_conf("NEWLIB_LIBC", True)
    zephyr_add_prj_conf("CONFIG_FPU", True)
    zephyr_add_prj_conf("NEWLIB_LIBC_FLOAT_PRINTF", True)
    zephyr_add_prj_conf("CPLUSPLUS", True)
    zephyr_add_prj_conf("LIB_CPLUSPLUS", True)
    # preferences
    zephyr_add_prj_conf("SETTINGS", True)
    zephyr_add_prj_conf("NVS", True)
    # watchdog
    zephyr_add_prj_conf("WATCHDOG", True)
    zephyr_add_prj_conf("WDT_DISABLE_AT_BOOT", False)
    # disable console
    zephyr_add_prj_conf("UART_CONSOLE", False)
    zephyr_add_prj_conf("CONSOLE", False)
    # TODO debug only
    zephyr_add_prj_conf("DEBUG_THREAD_INFO", True)
    # zephyr_add_prj_conf("DEBUG", True)
    ###
    zephyr_add_prj_conf("USE_SEGGER_RTT", True)
    zephyr_add_prj_conf("RTT_CONSOLE", True)
    zephyr_add_prj_conf("LOG", True)
    zephyr_add_prj_conf("LOG_BLOCK_IN_THREAD", True)
    zephyr_add_prj_conf("LOG_BUFFER_SIZE", 4096)
    zephyr_add_prj_conf("SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL", True)

    zephyr_add_prj_conf("USB_CDC_ACM_LOG_LEVEL_WRN", True)


def _format_prj_conf_val(value: PrjConfValueType) -> str:
    if isinstance(value, bool):
        return "y" if value else "n"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, str):
        return f'"{value}"'
    raise ValueError


def zephyr_copy_files():
    want_opts = CORE.data[KEY_ZEPHYR][KEY_PRJ_CONF]
    contents = (
        "\n".join(
            f"{name}={_format_prj_conf_val(value)}"
            for name, value in sorted(want_opts.items())
        )
        + "\n"
    )

    write_file_if_changed(CORE.relative_build_path("zephyr/prj.conf"), contents)
    write_file_if_changed(
        CORE.relative_build_path("zephyr/app.overlay"),
        CORE.data[KEY_ZEPHYR][KEY_OVERLAY],
    )

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
