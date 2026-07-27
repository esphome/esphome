import logging
from pathlib import Path
import re
import subprocess
import textwrap
from typing import TypedDict

from esphome import git
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_LOG_LEVEL,
    CONF_MAC_ADDRESS,
    CONF_PASSWORD,
    CONF_PATH,
    CONF_REF,
    CONF_REFRESH,
    CONF_TYPE,
    CONF_URL,
    CONF_USERNAME,
    CONF_VARIANT,
    CONF_VERSION,
    CONF_WATCHDOG_TIMEOUT,
    CONF_WIFI,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_PLATFORM,
    PLATFORM_ZEPHYR,
    TYPE_GIT,
    TYPE_LOCAL,
)
from esphome.core import CORE, CoroPriority, EsphomeError, coroutine_with_priority
from esphome.helpers import copy_file_if_changed, rmtree, write_file_if_changed
from esphome.types import ConfigType
from esphome.writer import clean_cmake_cache

from .const import (
    CONF_BOARD_SOURCE,
    CONF_CDC_ACM,
    CONF_KCONFIG_OPTIONS,
    CONF_NINJA_VERSION,
    CONF_SDK_SOURCE,
    CONF_WEST_VERSION,
    KEY_BOARD,
    KEY_BOARD_ROOT,
    KEY_BOOTLOADER,
    KEY_EXTRA_BUILD_FILES,
    KEY_KCONFIG,
    KEY_OVERLAY,
    KEY_PM_STATIC,
    KEY_PRJ_CONF,
    KEY_SDK_SOURCE,
    KEY_SYSBUILD_CONF,
    KEY_USER,
    KEY_ZEPHYR,
    ZEPHYR_VARIANT_ESP32,
    ZEPHYR_VARIANT_ESP32_C6,
    ZEPHYR_VARIANT_ESP32_H2,
    ZEPHYR_VARIANT_NATIVE_SIM,
    zephyr_ns,
)
from .gpio import zephyr_pin_to_code as _zephyr_pin_to_code  # noqa: F401
from .variants import VARIANTS

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@tomaszduda23"]
IS_TARGET_PLATFORM = True
AUTO_LOAD = ["preferences"]

PrjConfValueType = bool | str | int

# Mirrors esp32's LOG_LEVELS_IDF -- Zephyr's own native logging is a separate
# concern from ESPHome's `logger:` component.
LOG_LEVELS_ZEPHYR = [
    "NONE",
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG",
    "VERBOSE",
]

# Zephyr's CONFIG_LOG_DEFAULT_LEVEL is a plain int (0=off .. 4=debug), unlike
# ESP-IDF's per-level bool choice -- DEBUG/VERBOSE both collapse to Zephyr's max.
_ZEPHYR_LOG_LEVEL_TO_INT = {
    "NONE": 0,
    "ERROR": 1,
    "WARN": 2,
    "INFO": 3,
    "DEBUG": 4,
    "VERBOSE": 4,
}


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
    board_root: Path | None  # resolved zephyr: board_source: dir, or None if unset
    sdk_source: ConfigType | None  # validated zephyr: sdk_source:, or None if unset
    bootloader: str
    variant: str | None
    prj_conf: dict[str, dict[str, tuple[PrjConfValueType, bool]]]
    sysbuild_conf: dict[str, tuple[PrjConfValueType, bool]]
    overlay: dict[str, str]
    extra_build_files: dict[str, Path]
    pm_static: list[Section]
    user: dict[str, list[str]]
    kconfig: str
    fake_board_manifest: str | None
    dts_base_path: (
        str | None
    )  # local path to zephyr/ tree root with boards/; set by dts_fetch
    i2c_bus_cache: dict[
        str, object
    ]  # board -> list[str] | _NOT_FOUND; cleared each run
    cpp_path: str | None  # "" = unchecked; None = not found; else = executable path
    board_dir_cache: dict[str, str]  # board -> abs path str, "" = not found
    dts_include_paths: list[str] | None  # None = not yet computed
    board_edt_cache: dict[str, object]  # board -> EDT | _NOT_FOUND; cleared each run
    board_yaml_cache: dict[
        str, object
    ]  # board -> list[str] | _NOT_FOUND; cleared each run
    west_version: str | None  # None = use requirements_west.txt's pinned version
    ninja_version: str | None  # None = use requirements_west.txt's pinned version


# platform: nrf52 use only
def zephyr_set_core_data(config: ConfigType) -> None:
    CORE.data[KEY_ZEPHYR] = ZephyrData(
        board=config[CONF_BOARD],
        board_root=None,
        sdk_source=None,
        bootloader=config[KEY_BOOTLOADER],
        variant=None,
        prj_conf={},
        sysbuild_conf={},
        overlay={
            "": "",
        },  # set empty to make sure that overlay is cleared after config change
        extra_build_files={},
        pm_static=[],
        user={},
        kconfig="",
        fake_board_manifest=None,
        dts_base_path=None,
        i2c_bus_cache={},
        cpp_path="",
        board_dir_cache={},
        dts_include_paths=None,
        board_edt_cache={},
        board_yaml_cache={},
        west_version=None,
        ninja_version=None,
    )


def zephyr_data() -> ZephyrData:
    return CORE.data[KEY_ZEPHYR]


def zephyr_variant() -> str | None:
    """Return the active Zephyr variant name, or None if not yet set."""
    return zephyr_data().get("variant")


def zephyr_variant_family() -> str | None:
    """Return the active variant's silicon family (e.g. "esp32"), or None if unset/unfamilied."""
    variant = zephyr_variant()
    if variant is None:
        return None
    variant_info = VARIANTS.get(variant)
    return variant_info.family if variant_info is not None else None


def zephyr_dts_board_id(esphome_board: str) -> str:
    """Return the board id to use for DTS lookups (west's HWMv2 board/soc form on nrf52).

    Mirrors the west_board resolution in run_compile() -- NCS 3.3.0+ uses HWMv2
    board/soc names that don't match ESPHome's flat board id. Checks CORE.is_nrf52
    (not zephyr_variant(), which is never set for the standalone nrf52 platform).
    """
    if CORE.is_nrf52:
        from esphome.components.nrf52.boards import BOARDS_ZEPHYR  # noqa: PLC0415

        return BOARDS_ZEPHYR.get(esphome_board, {}).get("west_board", esphome_board)
    return esphome_board


def zephyr_only_on_variant(*variant_names: str):
    """Return a cv validator that requires the Zephyr variant to be one of variant_names."""

    def validator(value):
        variant = zephyr_variant()
        if variant not in variant_names:
            listed = ", ".join(repr(v) for v in variant_names)
            raise cv.Invalid(
                f"This feature requires Zephyr variant {listed}, "
                f"but the current variant is {variant!r}"
            )
        return value

    return validator


def zephyr_setup_i2c_pinctrl(
    board: str, bus_label: str, sda: int | None, scl: int | None
) -> tuple[int, int]:
    """Resolve I2C pin assignments and add the variant-specific pinctrl overlay.

    If sda/scl are not provided, attempts to read them from the board's DTS.
    Raises cv.Invalid if the pins cannot be determined. Returns (sda, scl).
    """
    variant_name = zephyr_data().get("variant") or ""

    if sda is None or scl is None:
        variant_info = VARIANTS.get(variant_name)
        extractor = (
            variant_info.pinctrl_extractors.get("i2c")
            if variant_info is not None
            else None
        )
        if extractor is not None:
            dts_pins = extractor(board, bus_label)
            if dts_pins is not None:
                sda = dts_pins.get("sda", sda)
                scl = dts_pins.get("scl", scl)
                _LOGGER.info(
                    "[zephyr] I2C pins for '%s' from DTS: SDA=GPIO%d SCL=GPIO%d",
                    board,
                    sda,
                    scl,
                )

    if sda is None or scl is None:
        raise cv.Invalid(
            "Could not determine I2C pin assignments for this board.\n"
            "Add explicit pin numbers to your i2c: configuration:\n"
            "  i2c:\n"
            "    sda: 26\n"
            "    scl: 27"
        )

    if variant_name == ZEPHYR_VARIANT_NATIVE_SIM:
        # No pinctrl node -- the emulated controller has no physical pins.
        zephyr_add_overlay(f'&{bus_label} {{ status = "okay"; }};')
    elif CORE.is_nrf52:
        # nRF52's TWIM has fully flexible pin muxing and no fixed I2C pins in the board
        # DTS, so a custom pinctrl overlay must be generated for whatever pins the user picked.
        zephyr_add_overlay(
            f"""
                &pinctrl {{
                    {bus_label}_default: {bus_label}_default {{
                        group1 {{
                            psels = <NRF_PSEL(TWIM_SDA, {sda // 32}, {sda % 32})>,
                                <NRF_PSEL(TWIM_SCL, {scl // 32}, {scl % 32})>;
                        }};
                    }};
                    {bus_label}_sleep: {bus_label}_sleep {{
                        group1 {{
                            psels = <NRF_PSEL(TWIM_SDA, {sda // 32}, {sda % 32})>,
                                <NRF_PSEL(TWIM_SCL, {scl // 32}, {scl % 32})>;
                            low-power-enable;
                        }};
                    }};
                }};
            """
        )
    elif zephyr_variant_family() == "esp32":
        # Override the board's fixed i2c*_default pinctrl node so the pins actually
        # used match the user's sda:/scl: YAML instead of the board's own defaults.
        prefix = bus_label.upper()
        pinctrl_overlay = f"""
            &pinctrl {{
                {bus_label}_default: {bus_label}_default {{
                    group1 {{
                        pinmux = <{prefix}_SDA_GPIO{sda}>,
                            <{prefix}_SCL_GPIO{scl}>;
                        bias-pull-up;
                        drive-open-drain;
                        output-high;
                    }};
                }};
            }};
        """
        if zephyr_variant() == ZEPHYR_VARIANT_ESP32:
            # Original ESP32 lacks hardware bus-clear support, so i2c_esp32.c needs
            # explicit sda-gpios/scl-gpios to recover a stuck bus in software. Every
            # other esp32-family chip has hardware bus-clear and treats these as a
            # hard #error instead.
            sda_ctlr, sda_pin = ("gpio1", sda - 32) if sda >= 32 else ("gpio0", sda)
            scl_ctlr, scl_pin = ("gpio1", scl - 32) if scl >= 32 else ("gpio0", scl)
            pinctrl_overlay += f"""
                &{bus_label} {{
                    sda-gpios = <&{sda_ctlr} {sda_pin} GPIO_OPEN_DRAIN>;
                    scl-gpios = <&{scl_ctlr} {scl_pin} GPIO_OPEN_DRAIN>;
                }};
            """
        zephyr_add_overlay(pinctrl_overlay)
    # Other variants: pinctrl already defined in board DTS; no overlay needed.

    return sda, scl


def zephyr_add_prj_conf(
    name: str,
    value: PrjConfValueType,
    required: bool = True,
    image: str = "",
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


def zephyr_set_prj_conf_override(
    name: str, value: PrjConfValueType, image: str = ""
) -> None:
    """Unconditionally set a zephyr prj conf value, replacing anything set by a component.

    Used only for user-supplied `kconfig_options:` (YAML), which must always win over a
    value set by component Python -- see the CoroPriority.FINAL job that calls this.
    """
    if not name.startswith("CONFIG_"):
        name = "CONFIG_" + name
    if image not in zephyr_data()[KEY_PRJ_CONF]:
        zephyr_data()[KEY_PRJ_CONF][image] = {}
    zephyr_data()[KEY_PRJ_CONF][image][name] = (value, True)


def zephyr_configure_net_contexts() -> None:
    """Size NET_MAX_CONTEXTS from real socket demand. Mirrors esp32's
    _configure_lwip_max_sockets()/LWIP_MAX_SOCKETS."""
    from esphome.components.socket import get_socket_counts  # noqa: PLC0415

    sc = get_socket_counts()
    total_sockets = max(sc.tcp + sc.udp + sc.tcp_listen, 10)
    zephyr_add_prj_conf("NET_MAX_CONTEXTS", total_sockets)
    # poll() max-fds Kconfig moved to the zvfs subsystem after nrf52's older NCS
    # branched off; that tree still only has the pre-rename name.
    if CORE.is_nrf52:
        zephyr_add_prj_conf("NET_SOCKETS_POLL_MAX", total_sockets)
    else:
        zephyr_add_prj_conf("ZVFS_POLL_MAX", total_sockets)


def zephyr_add_sysbuild_conf(
    name: str,
    value: PrjConfValueType,
    required: bool = True,
) -> None:
    """Set a sysbuild-level Kconfig value (e.g. SB_CONFIG_BOOTLOADER_MCUBOOT).

    Distinct from zephyr_add_prj_conf(): sysbuild Kconfig (SB_CONFIG_*) controls
    which child images sysbuild builds at all (e.g. whether an MCUboot image
    exists) and lives in its own zephyr/sysbuild.conf file, separate from any
    image's own CONFIG_* prj.conf.
    """
    if not name.startswith("SB_CONFIG_"):
        name = "SB_CONFIG_" + name
    sysbuild_conf = zephyr_data()[KEY_SYSBUILD_CONF]
    if name not in sysbuild_conf:
        sysbuild_conf[name] = (value, required)
        return
    old_value, old_required = sysbuild_conf[name]
    if old_value != value and old_required:
        raise ValueError(
            f"{name} already set with value '{old_value}', cannot set again to '{value}'"
        )
    if required:
        sysbuild_conf[name] = (value, required)


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


def _filter_source_files() -> list[str]:
    # i2c_emulator.cpp/.h are auto-discovered for every Zephyr build, but only make sense
    # when i2c: emulation: is configured (CONFIG_I2C_EMUL). Without this filter, sysbuild's
    # --whole-archive link force-pulls i2c_emul_register() even when unused, breaking the
    # link for real-hardware i2c: builds.
    if "i2c_emulator.cpp" not in zephyr_data()[KEY_EXTRA_BUILD_FILES]:
        return ["i2c_emulator.cpp", "i2c_emulator.h"]
    return []


FILTER_SOURCE_FILES = _filter_source_files


def zephyr_to_code(config: ConfigType) -> None:
    cg.add_build_flag("-DUSE_ZEPHYR")
    cg.add_define("USE_NATIVE_64BIT_TIME")
    cg.set_cpp_standard("gnu++20")
    if zephyr_variant() == ZEPHYR_VARIANT_NATIVE_SIM:
        # native_sim: use host glibc + libstdc++, avoiding picolibc/glibc type conflicts.
        zephyr_add_prj_conf("EXTERNAL_LIBC", True)
        zephyr_add_prj_conf("CPP", True)
        zephyr_add_prj_conf("EXTERNAL_LIBCPP", True)
    elif zephyr_variant_family() == "esp32":
        # REQUIRES_FULL_LIBCPP selects GLIBCXX_LIBCPP; without it Zephyr defaults to
        # MINIMAL_LIBCPP, which has no STL and breaks ESPHome's C++ headers.
        zephyr_add_prj_conf("CPP", True)
        zephyr_add_prj_conf("REQUIRES_FULL_LIBCPP", True)
        # Consumed by C++ code shared across every esp32-family variant (core.cpp, etc.).
        cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_FAMILY_ESP32")
    else:
        # No zephyr variant: platform: nrf52 calling this shared helper directly, uses newlib.
        zephyr_add_prj_conf("NEWLIB_LIBC", True)
        zephyr_add_prj_conf("NEWLIB_LIBC_FLOAT_PRINTF", True)

    # esp32_h2/esp32_c6 are RV32IMAC -- no hardware FPU. Original ESP32 is Xtensa LX6,
    # which does have one (soc/espressif/esp32/Kconfig selects CPU_HAS_FPU), so it can't
    # be excluded by family the way the RISC-V esp32-family chips are.
    if zephyr_variant() not in (ZEPHYR_VARIANT_ESP32_H2, ZEPHYR_VARIANT_ESP32_C6):
        zephyr_add_prj_conf("FPU", True)
    zephyr_add_prj_conf("STD_CPP20", True)
    # random_bytes() uses sys_rand_get() which requires the entropy subsystem
    zephyr_add_prj_conf("ENTROPY_GENERATOR", True)
    # <err> os: ***** USAGE FAULT *****
    # <err> os:   Illegal load of EXC_RETURN into PC
    zephyr_add_prj_conf("MAIN_STACK_SIZE", 4096, required=False)
    zephyr_add_prj_conf("SYSTEM_WORKQUEUE_STACK_SIZE", 2048, required=False)
    if CONF_WIFI in CORE.config:
        # Doubles Zephyr's default stack size for WiFi's dynamically-spawned worker
        # threads. Gated on wifi: since DYNAMIC_THREAD is only selected when WiFi is enabled.
        zephyr_add_prj_conf("DYNAMIC_THREAD_STACK_SIZE", 2048, required=False)

    # zephyr_variant() is None for platform: nrf52, which manages its own watchdog
    # setup separately (nrf52/__init__.py) but defines the same consumer macro.
    # native_sim has no real watchdog hardware.
    if zephyr_variant() is not None and zephyr_variant() != ZEPHYR_VARIANT_NATIVE_SIM:
        zephyr_add_prj_conf("WATCHDOG", True)
        zephyr_add_prj_conf("WDT_DISABLE_AT_BOOT", False)
        timeout_ms = int(config[CONF_WATCHDOG_TIMEOUT].total_milliseconds)
        cg.add_define("USE_ZEPHYR_WATCHDOG_TIMEOUT_MS", timeout_ms)
        # Identifies the stalled thread on the console from the watchdog callback (see
        # hal.cpp) -- without THREAD_NAME, k_thread_name_get() just returns NULL.
        zephyr_add_prj_conf("THREAD_NAME", True)
        # arch_stack_walk() isn't implemented for Xtensa (original ESP32); the call
        # would silently do nothing there.
        if zephyr_variant() != ZEPHYR_VARIANT_ESP32:
            cg.add_define("USE_ZEPHYR_ARCH_STACKWALK")

    # .get(): nrf52's config dict has no log_level key yet, falls back to the same
    # default as a genuine platform: zephyr block.
    log_level = config.get(CONF_LOG_LEVEL, "ERROR")
    if log_level != "NONE":
        zephyr_add_prj_conf("LOG", True)
        zephyr_add_prj_conf("LOG_DEFAULT_LEVEL", _ZEPHYR_LOG_LEVEL_TO_INT[log_level])
        # Normally pulled in transitively by LOG_BACKEND_UART, which the logger component
        # disables in favor of its own backend, so it must be set explicitly here.
        zephyr_add_prj_conf("LOG_OUTPUT", True)

        # Both default to y already, set explicitly so a crash dumps a backtrace instead
        # of looking like a silent reboot.
        zephyr_add_prj_conf("EXCEPTION_DEBUG", True)
        # EXCEPTION_STACK_TRACE needs ARCH_STACKWALK, which has no arch_stack_walk()
        # for Xtensa (original ESP32) or POSIX (native_sim) -- excluded so the Kconfig
        # doesn't warn about a backtrace that can never happen. nrf52 is excluded too:
        # its older NCS doesn't select ARCH_STACKWALK, making this a fatal Kconfig
        # error there instead of a no-op.
        if zephyr_variant() is not None and zephyr_variant() not in (
            ZEPHYR_VARIANT_ESP32,
            ZEPHYR_VARIANT_NATIVE_SIM,
        ):
            zephyr_add_prj_conf("EXCEPTION_STACK_TRACE", True)

    CORE.add_job(_kconfig_options_to_code, config)
    CORE.add_job(_cdc_acm_to_code, config)


@coroutine_with_priority(CoroPriority.FINAL)
async def _kconfig_options_to_code(config: ConfigType) -> None:
    # .get(): nrf52's config dict has no kconfig_options key.
    #
    # Runs at the lowest priority so every component has already set its own prj.conf
    # defaults -- user-supplied kconfig_options: must always win, so this overrides
    # unconditionally instead of going through zephyr_add_prj_conf()'s conflict check.
    for name, value in config.get(CONF_KCONFIG_OPTIONS, {}).items():
        if isinstance(value, dict):
            for image_name, image_value in value.items():
                zephyr_set_prj_conf_override(image_name, image_value, image=name)
        else:
            zephyr_set_prj_conf_override(name, value)


@coroutine_with_priority(CoroPriority.FINAL)
async def _cdc_acm_to_code(config: ConfigType) -> None:
    if "CONFIG_CDC_ACM_DTE_RATE_CALLBACK_SUPPORT" in zephyr_data()[KEY_PRJ_CONF][""]:
        var = cg.new_Pvariable(config[CONF_CDC_ACM])
        await cg.register_component(var, {})


def zephyr_setup_preferences():
    cg.add(zephyr_ns.setup_preferences())
    zephyr_add_prj_conf("SETTINGS", True)
    if zephyr_variant() == ZEPHYR_VARIANT_NATIVE_SIM:
        zephyr_add_prj_conf("SETTINGS_RUNTIME", True)
    else:
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


def _write_file_if_changed_or_remove_when_empty(path: Path, content: str) -> bool:
    """Write content to path, or remove a stale file when content is empty.

    Returns True if the file changed on disk.
    """
    if content:
        return write_file_if_changed(path, content)
    if path.is_file():
        path.unlink()
        return True
    return False


def copy_files() -> None:
    changed = False

    if manifest := zephyr_data()["fake_board_manifest"]:
        changed |= write_file_if_changed(
            CORE.relative_build_path(f"boards/{zephyr_data()[KEY_BOARD]}.json"),
            manifest,
        )

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
            path = CORE.relative_build_path(f"zephyr/sysbuild/{image}.conf")
        else:
            path = CORE.relative_build_path("zephyr/prj.conf")

        changed |= write_file_if_changed(path, prj_conf)

    for image, content in zephyr_data()[KEY_OVERLAY].items():
        if image:
            path = CORE.relative_build_path(f"zephyr/sysbuild/{image}.overlay")
        else:
            path = CORE.relative_build_path("zephyr/app.overlay")
        changed |= write_file_if_changed(path, content)

    for filename, path in zephyr_data()[KEY_EXTRA_BUILD_FILES].items():
        changed |= copy_file_if_changed(
            path,
            CORE.relative_build_path(filename),
        )

    pm_static = "\n".join(str(item) for item in zephyr_data()[KEY_PM_STATIC])
    changed |= _write_file_if_changed_or_remove_when_empty(
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
    changed |= _write_file_if_changed_or_remove_when_empty(
        CORE.relative_build_path("zephyr/Kconfig"), kconfig
    )

    sysbuild_conf_opts = zephyr_data()[KEY_SYSBUILD_CONF]
    sysbuild_conf_lines = [
        f"{name}={_format_prj_conf_val(value[0])}"
        for name, value in sorted(sysbuild_conf_opts.items())
    ]
    sysbuild_conf = "\n".join(sysbuild_conf_lines) + "\n" if sysbuild_conf_lines else ""
    changed |= _write_file_if_changed_or_remove_when_empty(
        CORE.relative_build_path("zephyr/sysbuild.conf"), sysbuild_conf
    )

    if changed:
        # A configure-time input changed; drop the CMake cache so the build can't reuse
        # stale configure results -- see clean_cmake_cache() in esphome/writer.py.
        clean_cmake_cache()


# Watchdog range mirrors esp32's own CONF_WATCHDOG_TIMEOUT validator (esp32/__init__.py) --
# confirmed working on real ESP32-H2 hardware under Zephyr.
_WATCHDOG_TIMEOUT_VALIDATOR = cv.All(
    cv.positive_time_period_seconds,
    cv.Range(min=cv.TimePeriod(seconds=5), max=cv.TimePeriod(seconds=60)),
)

# Like cv.SOURCE_SCHEMA's git shape, but `url` is optional: `sdk_source: {ref: ...}`
# alone is enough to pin an alpha/pre-release branch of the variant's own official
# manifest (see _variant_config_schema, which fills in the default). board_source
# keeps the generic cv.SOURCE_SCHEMA since it has no equivalent "official" default.
_SDK_SOURCE_GIT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_URL): cv.url,
        cv.Optional(CONF_REF): cv.git_ref,
        cv.Optional(CONF_USERNAME): cv.string,
        cv.Optional(CONF_PASSWORD): cv.string,
        cv.Optional(CONF_PATH): cv.string,
    }
)
_SDK_SOURCE_SCHEMA = cv.Any(
    cv.validate_source_shorthand,
    cv.typed_schema(
        {
            TYPE_GIT: _SDK_SOURCE_GIT_SCHEMA,
            TYPE_LOCAL: cv.LOCAL_SCHEMA,
        }
    ),
)

# Not applied when zephyr is auto-loaded as a shared dependency (e.g. by nrf52) --
# see _variant_config_schema.
_ZEPHYR_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_VARIANT): cv.one_of(*VARIANTS, upper=True),
        cv.Optional(CONF_BOARD): cv.string_strict,
        cv.Optional(CONF_BOARD_SOURCE): cv.SOURCE_SCHEMA,
        cv.Optional(CONF_REFRESH, default="1d"): cv.All(cv.string, cv.source_refresh),
        # version still gates the semver used for feature checks throughout this
        # component; sdk_source only changes *where* the SDK is fetched from.
        # Also accepts the literal "recommended", an explicit alias for the variant's
        # default_version (same effect as omitting this key).
        cv.Optional(CONF_VERSION): cv.string_strict,
        # Overrides the official zephyrproject-rtos/zephyr manifest with a fork/branch/ref
        # or a `local:` path to an already-initialized west workspace. `url` may be
        # omitted to target a branch (e.g. an alpha/pre-release) of the variant's own
        # official manifest -- see _variant_config_schema.
        cv.Optional(CONF_SDK_SOURCE): _SDK_SOURCE_SCHEMA,
        # Overrides requirements_west.txt's pinned west/ninja version, decoupled from
        # CONF_VERSION since their release cadence isn't tied to Zephyr's.
        cv.Optional(CONF_WEST_VERSION): cv.string_strict,
        cv.Optional(CONF_NINJA_VERSION): cv.string_strict,
        cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
        cv.Optional(CONF_WATCHDOG_TIMEOUT): _WATCHDOG_TIMEOUT_VALIDATOR,
        # Zephyr's own native logging verbosity (CONFIG_LOG_DEFAULT_LEVEL) -- separate
        # from ESPHome's own `logger: level:`, mirroring esp32's `framework: log_level:`.
        cv.Optional(CONF_LOG_LEVEL, default="ERROR"): cv.one_of(
            *LOG_LEVELS_ZEPHYR, upper=True
        ),
        # Raw Kconfig passthrough -- Zephyr's equivalent of esp32's sdkconfig_options. A
        # dict value (e.g. `mcuboot:`) targets that sysbuild child image's own prj.conf.
        cv.Optional(CONF_KCONFIG_OPTIONS, default={}): {
            cv.string_strict: cv.Any(
                cv.boolean,
                cv.int_,
                cv.string_strict,
                {cv.string_strict: cv.Any(cv.boolean, cv.int_, cv.string_strict)},
            )
        },
    }
)


def _resolve_board_source(config: ConfigType, board: str) -> Path:
    """Resolve zephyr: board_source: into a local BOARD_ROOT directory."""
    conf = config[CONF_BOARD_SOURCE]
    if conf[CONF_TYPE] == TYPE_GIT:
        with cv.prepend_path([CONF_BOARD_SOURCE]):
            root, _ = git.clone_or_update(
                url=conf[CONF_URL],
                ref=conf.get(CONF_REF),
                refresh=config[CONF_REFRESH],
                domain=KEY_ZEPHYR,
                username=conf.get(CONF_USERNAME),
                password=conf.get(CONF_PASSWORD),
            )
            if path := conf.get(CONF_PATH):
                root = root / path
    elif conf[CONF_TYPE] == TYPE_LOCAL:
        root = Path(CORE.relative_config_path(conf[CONF_PATH]))
    else:
        raise NotImplementedError

    # board.yml lives under boards/<vendor>/<bare name>/, even if `board:` is fully qualified.
    board_name = board.split("/", 1)[0]
    if not list(root.glob(f"boards/*/{board_name}/board.yml")):
        raise cv.Invalid(
            f"Could not find boards/*/{board_name}/board.yml under this board_source. "
            f"Please check the source contains that board.",
            [CONF_BOARD_SOURCE],
        )
    return root


def _variant_config_schema(config: ConfigType) -> ConfigType:
    # CORE.is_zephyr can't be used here: for a genuine `platform: zephyr` config, this
    # very function is what sets target_platform. Only skip when some OTHER platform
    # has already claimed it (e.g. nrf52, before auto-loading zephyr as a dependency).
    target_platform = CORE.data.get(KEY_CORE, {}).get(KEY_TARGET_PLATFORM)
    if target_platform is not None and target_platform != PLATFORM_ZEPHYR:
        # Auto-loaded as a shared dependency (e.g. by nrf52) -- skip variant/board
        # validation, the owning platform component populates CORE.data[KEY_ZEPHYR] itself.
        return config
    config = _ZEPHYR_SCHEMA(config)
    if CONF_VERSION in config and CONF_SDK_SOURCE in config:
        raise cv.Invalid(
            f"'{CONF_VERSION}' and '{CONF_SDK_SOURCE}' are mutually exclusive -- "
            f"'{CONF_SDK_SOURCE}' determines the version from the source itself.",
            [CONF_SDK_SOURCE],
        )
    variant = config[CONF_VARIANT]
    if CONF_SDK_SOURCE in config:
        from .dts_fetch import (  # deferred: avoid import cycle
            resolve_sdk_source_version,
        )

        source = config[CONF_SDK_SOURCE]
        if source[CONF_TYPE] == TYPE_GIT and CONF_URL not in source:
            source[CONF_URL] = VARIANTS[variant].sdk.manifest_url
        with cv.prepend_path([CONF_SDK_SOURCE]):
            config[KEY_FRAMEWORK_VERSION] = resolve_sdk_source_version(
                source, config[CONF_REFRESH]
            )
    if CONF_MAC_ADDRESS in config and variant != ZEPHYR_VARIANT_NATIVE_SIM:
        raise cv.Invalid(
            f"'{CONF_MAC_ADDRESS}' is only valid for variant "
            f"'{ZEPHYR_VARIANT_NATIVE_SIM}', but the current variant is {variant!r}",
            [CONF_MAC_ADDRESS],
        )
    if variant == ZEPHYR_VARIANT_NATIVE_SIM:
        if CONF_WATCHDOG_TIMEOUT in config:
            raise cv.Invalid(
                f"'{CONF_WATCHDOG_TIMEOUT}' is not valid for variant "
                f"'{ZEPHYR_VARIANT_NATIVE_SIM}' (no real watchdog hardware)",
                [CONF_WATCHDOG_TIMEOUT],
            )
    else:
        config.setdefault(CONF_WATCHDOG_TIMEOUT, cv.TimePeriod(seconds=5))
    board_root = None
    if CONF_BOARD_SOURCE in config:
        if CONF_BOARD not in config:
            raise cv.Invalid(
                f"'{CONF_BOARD_SOURCE}' requires '{CONF_BOARD}' to also be set",
                [CONF_BOARD_SOURCE],
            )
        board_root = _resolve_board_source(config, config[CONF_BOARD])
    elif CONF_BOARD in config:
        valid_boards = VARIANTS[variant].boards
        board = config[CONF_BOARD]
        if valid_boards and board not in valid_boards:
            raise cv.Invalid(
                f"Board {board!r} is not valid for variant {variant!r}. "
                f"See https://esphome.io/components/zephyr.html for valid boards.",
                [CONF_BOARD],
            )
    if variant == ZEPHYR_VARIANT_ESP32:
        from .variants.esp32 import config_schema as _esp32_config_schema

        config = _esp32_config_schema(config)
    elif variant == ZEPHYR_VARIANT_ESP32_H2:
        from .variants.esp32_h2 import config_schema as _esp32_h2_config_schema

        config = _esp32_h2_config_schema(config)
    elif variant == ZEPHYR_VARIANT_ESP32_C6:
        from .variants.esp32_c6 import config_schema as _esp32_c6_config_schema

        config = _esp32_c6_config_schema(config)
    elif variant == ZEPHYR_VARIANT_NATIVE_SIM:
        from .variants.native_sim import config_schema as _native_sim_config_schema

        config = _native_sim_config_schema(config)
    else:
        raise cv.Invalid(f"Variant {variant!r} has no config schema registered yet")
    zephyr_data()[KEY_BOARD_ROOT] = board_root
    zephyr_data()[KEY_SDK_SOURCE] = config.get(CONF_SDK_SOURCE)
    return config


CONFIG_SCHEMA = _variant_config_schema


@coroutine_with_priority(CoroPriority.PLATFORM)
async def to_code(config: ConfigType) -> None:
    if not CORE.is_zephyr:
        # Auto-loaded as a shared dependency (e.g. by nrf52); the owning platform
        # component calls zephyr_to_code() itself, so there's nothing to do here.
        return

    from .dts_fetch import fetch_board_dts

    variant = zephyr_data()["variant"]
    await fetch_board_dts(variant, zephyr_data()["sdk_source"], config[CONF_REFRESH])

    from .dts_lookup import log_board_capabilities, validate_board

    board = zephyr_data()["board"]
    if validate_board(board) is False:
        raise EsphomeError(
            f"Board '{board}' was not found. Check the board name, or "
            f"'{CONF_BOARD_SOURCE}' if using a custom board."
        )
    log_board_capabilities(
        board,
        variant,
        VARIANTS[variant],
        str(CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]),
        zephyr_data()["board_root"],
    )

    if variant == ZEPHYR_VARIANT_ESP32:
        from .variants.esp32 import to_code as _esp32_to_code

        await _esp32_to_code(config)
        return
    if variant == ZEPHYR_VARIANT_ESP32_H2:
        from .variants.esp32_h2 import to_code as _esp32_h2_to_code

        await _esp32_h2_to_code(config)
        return
    if variant == ZEPHYR_VARIANT_ESP32_C6:
        from .variants.esp32_c6 import to_code as _esp32_c6_to_code

        await _esp32_c6_to_code(config)
        return
    if variant == ZEPHYR_VARIANT_NATIVE_SIM:
        from .variants.native_sim import to_code as _native_sim_to_code

        await _native_sim_to_code(config)
        return
    raise NotImplementedError(f"Zephyr variant {variant!r} has no to_code registered")


def upload_program(config: ConfigType, args, host: str) -> bool:
    # Every esp32-family variant flashes the same way: Zephyr's generic esp32
    # runner (runners/esp32.py, wrapping esptool) via `west flash`.
    if zephyr_variant_family() != "esp32":
        return False  # no custom uploader for this variant yet; fall through to default OTA

    from esphome.upload_targets import PortType, get_port_type

    if get_port_type(host) != PortType.SERIAL:
        return False  # only serial (esptool via west) is implemented so far

    from esphome.__main__ import check_permissions

    check_permissions(host)

    from .build_zephyr import run_west_flash
    from .framework_west import check_and_install as west_install

    version = str(CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION])
    variant_data = VARIANTS[zephyr_variant()]
    python_bin, framework_path, west_env = west_install(
        variant_data.sdk,
        version,
        zephyr_data()["west_version"],
        zephyr_data()["ninja_version"],
        zephyr_data()["sdk_source"],
        config[CORE.target_platform][CONF_REFRESH],
    )

    build_dir = CORE.relative_build_path(".west_build")
    speed = getattr(args, "upload_speed", None)

    if not run_west_flash(python_bin, framework_path, west_env, build_dir, host, speed):
        raise EsphomeError("Zephyr west flash failed")
    return True


def run_compile(args, config: ConfigType) -> bool:
    from .library import generate_zephyr_modules

    variant = zephyr_data()["variant"]
    extra_modules = generate_zephyr_modules(list(CORE.platformio_libraries.values()))

    if not CORE.using_toolchain_sdk_zephyr:
        return False

    from .build_zephyr import (
        _VARIANT_TOOLCHAIN,
        generate_cmake_lists,
        run_west_blobs_fetch,
        run_west_build,
    )
    from .framework_west import check_and_install as west_install
    from .sdk_setup_west import check_and_install as sdk_install

    variant_data = VARIANTS[variant]
    version = str(CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION])
    python_bin, framework_path, west_env = west_install(
        variant_data.sdk,
        version,
        zephyr_data()["west_version"],
        zephyr_data()["ninja_version"],
        zephyr_data()["sdk_source"],
        config[CORE.target_platform][CONF_REFRESH],
    )
    cross_toolchain = _VARIANT_TOOLCHAIN.get(variant)
    sdk_dir = sdk_install(framework_path, toolchain=cross_toolchain)
    if blob_spec := variant_data.blobs:
        module, allow_regex, sentinel_name = blob_spec
        run_west_blobs_fetch(
            python_bin, framework_path, west_env, module, allow_regex, sentinel_name
        )
    cmake_lists_changed = generate_cmake_lists(mode=variant)
    board = zephyr_data()[KEY_BOARD]
    zephyr_toolchain_variant = "zephyr" if cross_toolchain else "host"

    # West can't detect a missing CMake cache itself -- its pristine modes read
    # ZEPHYR_BASE from the very cache that was dropped. build_dir here must match
    # run_west_build()'s own build_dir selection exactly.
    build_dir = (
        CORE.relative_build_path(".west_build")
        if sdk_dir is not None
        else CORE.relative_pioenvs_path(CORE.name)
    )
    if (
        cmake_lists_changed or not (build_dir / "CMakeCache.txt").is_file()
    ) and build_dir.is_dir():
        _LOGGER.info("Build inputs changed, cleaning %s", build_dir)
        rmtree(build_dir)

    run_west_build(
        python_bin,
        framework_path,
        board,
        west_env,
        sdk_install_dir=sdk_dir,
        zephyr_toolchain_variant=zephyr_toolchain_variant,
        extra_modules=extra_modules,
        board_root=zephyr_data().get(KEY_BOARD_ROOT),
    )
    return True


def _addr2line(addr2line: str, elf: Path, addr: str) -> str:
    try:
        result = subprocess.run(
            [addr2line, "-e", elf, addr],
            capture_output=True,
            text=True,
            check=True,
        )
        return result.stdout.strip().splitlines()[0]
    except Exception as err:  # noqa: BLE001  # pylint: disable=broad-except
        _LOGGER.error("Running command failed: %s", err)
    return ""


def process_stacktrace(config: ConfigType, line: str, backtrace_state: bool) -> bool:
    if "Last crash:" in line:
        return True
    if backtrace_state:
        match = re.search(r"PC=(0x[0-9a-fA-F]+)\s+LR=(0x[0-9a-fA-F]+)", line)
        if match:
            pc = match.group(1)
            lr = match.group(2)
            from esphome.analyze_memory.toolchain import find_tool

            addr2line = find_tool("addr2line")
            if addr2line is None:
                return False
            # --sysbuild nests the app image under an extra "zephyr" domain
            # directory on top of Zephyr's own standard "zephyr" build-output
            # subdirectory (same layout confirmed for native_sim's zephyr.exe
            # in esphome/core/__init__.py's firmware_bin).
            elf = CORE.relative_build_path(
                ".west_build", "zephyr", "zephyr", "zephyr.elf"
            )
            if not elf.exists():
                _LOGGER.warning("%s does not exists", elf)
                return False
            _LOGGER.error("=== CRASH ===")
            _LOGGER.error("PC: %s", _addr2line(addr2line, elf, pc))
            _LOGGER.error("LR: %s", _addr2line(addr2line, elf, lr))
    return False
