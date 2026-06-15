from pathlib import Path
import textwrap
from typing import TypedDict

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_BOARD, KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.helpers import copy_file_if_changed, write_file_if_changed
from esphome.types import ConfigType

from .const import (
    CONF_CDC_ACM,
    KEY_BOOTLOADER,
    KEY_EXTRA_BUILD_FILES,
    KEY_KCONFIG,
    KEY_OVERLAY,
    KEY_PM_STATIC,
    KEY_PRJ_CONF,
    KEY_USER,
    KEY_ZEPHYR,
    zephyr_ns,
)

CODEOWNERS = ["@tomaszduda23"]

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
    prj_conf: dict[str, dict[str, tuple[PrjConfValueType, bool]]]
    overlay: dict[str, str]
    extra_build_files: dict[str, Path]
    pm_static: list[Section]
    user: dict[str, list[str]]
    kconfig: str


def zephyr_set_core_data(config: ConfigType) -> None:
    CORE.data[KEY_ZEPHYR] = ZephyrData(
        board=config[CONF_BOARD],
        bootloader=config[KEY_BOOTLOADER],
        prj_conf={},
        overlay={
            "": "",
        },  # set empty to make sure that overlay is cleared after config change
        extra_build_files={},
        pm_static=[],
        user={},
        kconfig="",
    )


def zephyr_data() -> ZephyrData:
    return CORE.data[KEY_ZEPHYR]


def zephyr_add_prj_conf(
    name: str, value: PrjConfValueType, required: bool = True, image: str = ""
) -> None:
    """Set an zephyr prj conf value."""
    if not name.startswith("CONFIG_"):
        name = "CONFIG_" + name
    if image not in zephyr_data()[KEY_PRJ_CONF]:
        zephyr_data()[KEY_PRJ_CONF][image] = {}
    prj_conf = zephyr_data()[KEY_PRJ_CONF][image]
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


def zephyr_add_overlay(content: str, image: str = "") -> None:
    data = zephyr_data()
    if image not in data[KEY_OVERLAY]:
        data[KEY_OVERLAY][image] = ""
    data[KEY_OVERLAY][image] += textwrap.dedent(content)


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


def zephyr_to_code(config: ConfigType) -> None:
    cg.add_build_flag("-DUSE_ZEPHYR")
    cg.add_define("USE_NATIVE_64BIT_TIME")
    cg.set_cpp_standard("gnu++20")
    # c++ support
    zephyr_add_prj_conf("NEWLIB_LIBC", True)
    zephyr_add_prj_conf("FPU", True)
    zephyr_add_prj_conf("NEWLIB_LIBC_FLOAT_PRINTF", True)
    zephyr_add_prj_conf("STD_CPP20", True)
    # random_bytes() uses sys_rand_get() which requires the entropy subsystem
    zephyr_add_prj_conf("ENTROPY_GENERATOR", True)

    # <err> os: ***** USAGE FAULT *****
    # <err> os:   Illegal load of EXC_RETURN into PC
    zephyr_add_prj_conf("MAIN_STACK_SIZE", 2048)

    CORE.add_job(_cdc_acm_to_code, config)


@coroutine_with_priority(CoroPriority.FINAL)
async def _cdc_acm_to_code(config: ConfigType) -> None:
    if "CONFIG_CDC_ACM_DTE_RATE_CALLBACK_SUPPORT" in zephyr_data()[KEY_PRJ_CONF][""]:
        var = cg.new_Pvariable(config[CONF_CDC_ACM])
        await cg.register_component(var, {})


def zephyr_setup_preferences():
    cg.add(zephyr_ns.setup_preferences())
    zephyr_add_prj_conf("SETTINGS", True)
    zephyr_add_prj_conf("NVS", True)
    zephyr_add_prj_conf("FLASH_MAP", True)
    zephyr_add_prj_conf("FLASH", True)


def _format_prj_conf_val(value: PrjConfValueType) -> str:
    if isinstance(value, bool):
        return "y" if value else "n"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, str):
        return f'"{value}"'
    raise ValueError


def zephyr_add_cdc_acm(config: ConfigType, id: int) -> None:
    framework_ver: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    if CORE.is_nrf52 and framework_ver >= cv.Version(3, 2, 0):
        zephyr_add_prj_conf("CONFIG_USB_DEVICE_STACK_NEXT", False)
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


def zephyr_add_kconfig(kconfig: str) -> None:
    zephyr_data()[KEY_KCONFIG] += textwrap.dedent(kconfig) + "\n"


def zephyr_add_pm_static(sections: list[Section]) -> None:
    zephyr_data()[KEY_PM_STATIC].extend(sections)


def zephyr_add_user(key, value):
    user = zephyr_data()[KEY_USER]
    if key not in user:
        user[key] = []
    user[key] += [value]


def copy_files():
    user = zephyr_data()[KEY_USER]
    if user:
        entries = " ".join(
            f"{key} = {', '.join(value)};" for key, value in user.items()
        )
        zephyr_add_overlay(
            f"""
                / {{
                    zephyr,user {{
                        {entries}
                    }};
                }};
            """
        )

    for image, want_opts in zephyr_data()[KEY_PRJ_CONF].items():
        prj_conf = (
            "\n".join(
                f"{name}={_format_prj_conf_val(value[0])}"
                for name, value in sorted(want_opts.items())
            )
            + "\n"
        )

        if image:
            path = CORE.relative_build_path(f"sysbuild/{image}.conf")
        else:
            path = CORE.relative_build_path("zephyr/prj.conf")

        write_file_if_changed(CORE.relative_build_path(path), prj_conf)

    for image, content in zephyr_data()[KEY_OVERLAY].items():
        if image:
            path = CORE.relative_build_path(f"sysbuild/{image}.overlay")
        else:
            path = CORE.relative_build_path("zephyr/app.overlay")
        write_file_if_changed(path, content)

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

    kconfig = zephyr_data()[KEY_KCONFIG]
    if kconfig:
        kconfig = (
            textwrap.dedent(
                """
                menu "Zephyr"
                source "Kconfig.zephyr"
                endmenu
                """
            )
            + "\n"
            + kconfig
        )
        write_file_if_changed(CORE.relative_build_path("zephyr/Kconfig"), kconfig)
