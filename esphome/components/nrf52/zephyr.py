import esphome.codegen as cg
from typing import Union
from esphome.core import CORE
from esphome.helpers import (
    write_file_if_changed,
)
from .const import (
    ZEPHYR_VARIANT_GENERIC,
    KEY_ZEPHYR,
    KEY_PRJ_CONF,
    KEY_OVERLAY,
    ZEPHYR_VARIANT_NRF_SDK,
)
from esphome.const import (
    CONF_VARIANT,
)


def zephyr_set_core_data(config):
    CORE.data[KEY_ZEPHYR] = {}
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
    else:
        raise NotImplementedError
    # c++ support
    zephyr_add_prj_conf("NEWLIB_LIBC", False)
    zephyr_add_prj_conf("NEWLIB_LIBC_NANO", True)
    zephyr_add_prj_conf("NEWLIB_LIBC_FLOAT_PRINTF", True)
    zephyr_add_prj_conf("CPLUSPLUS", True)
    zephyr_add_prj_conf("LIB_CPLUSPLUS", True)
    # watchdog
    zephyr_add_prj_conf("WATCHDOG", True)
    zephyr_add_prj_conf("WDT_DISABLE_AT_BOOT", False)
    # TODO debug only
    # zephyr_add_prj_conf("DEBUG_THREAD_INFO", True)


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
