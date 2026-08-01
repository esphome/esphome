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
    CONF_ADVANCED,
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_LOG_LEVEL,
    CONF_MAC_ADDRESS,
    CONF_PASSWORD,
    CONF_PATH,
    CONF_REF,
    CONF_REFRESH,
    CONF_SOURCE,
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
    CONF_OVERLAYS,
    CONF_SNIPPETS,
    CONF_WEST_VERSION,
    KEY_BOARD,
    KEY_BOARD_ROOT,
    KEY_BOOTLOADER,
    KEY_EXTRA_BUILD_FILES,
    KEY_FRAMEWORK_TYPE,
    KEY_KCONFIG,
    KEY_OVERLAY,
    KEY_PM_STATIC,
    KEY_PRJ_CONF,
    KEY_SNIPPETS,
    KEY_SYSBUILD_CONF,
    KEY_USER,
    KEY_ZEPHYR,
    ZEPHYR_VARIANT_ESP32,
    ZEPHYR_VARIANT_ESP32_C6,
    ZEPHYR_VARIANT_ESP32_H2,
    ZEPHYR_VARIANT_NATIVE_SIM,
    ZEPHYR_VARIANT_NRF52,
    ZEPHYR_VARIANT_NRF54L15,
    zephyr_ns,
)
from .gpio import zephyr_pin_to_code as _zephyr_pin_to_code  # noqa: F401
from .variants import VARIANTS, resolve_sdk

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@tomaszduda23"]
IS_TARGET_PLATFORM = True
AUTO_LOAD = ["preferences"]


class HexValue:
    """Wrap an integer so it is written as 0x... in prj.conf (required for hex Kconfig types)."""

    def __init__(self, value: int) -> None:
        self.value = value

    def __eq__(self, other: object) -> bool:
        if isinstance(other, HexValue):
            return self.value == other.value
        return NotImplemented

    def __repr__(self) -> str:
        return f"HexValue(0x{self.value:X})"

    def __str__(self) -> str:
        return f"0x{self.value:X}"


PrjConfValueType = bool | str | int | HexValue

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
    sdk_source: (
        ConfigType | None
    )  # resolved zephyr: framework: source:, or None if unset
    bootloader: str
    variant: str | None
    framework_type: str  # resolved zephyr: framework: type: (e.g. "zephyr", "ncs")
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
    snippets: list[
        str
    ]  # zephyr: snippets: -- one `-S <name>` per entry to `west build`


# platform: nrf52 use only
def zephyr_set_core_data(config: ConfigType) -> None:
    CORE.data[KEY_ZEPHYR] = ZephyrData(
        board=config[CONF_BOARD],
        board_root=None,
        sdk_source=None,
        bootloader=config[KEY_BOOTLOADER],
        variant=None,
        # platform: nrf52 is inherently NCS-based (no alternate).
        framework_type="ncs",
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
        snippets=[],
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
    elif CORE.is_nrf52 or zephyr_variant_family() == "nordic":
        # nRF52's TWIM has fully flexible pin muxing and no fixed I2C pins in the board
        # DTS, so a custom pinctrl overlay must be generated for whatever pins the user picked.
        # Same devicetree shape whether this is platform: nrf52 or platform: zephyr's
        # nordic-family variant -- identical physical TWIM peripheral either way.
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
    elif zephyr_variant_family() == "nordic":
        # Same reasoning as esp32-family above: mainline Zephyr's MINIMAL_LIBCPP has no
        # STL, which ESPHome's C++ core requires regardless of chip vendor.
        zephyr_add_prj_conf("CPP", True)
        zephyr_add_prj_conf("REQUIRES_FULL_LIBCPP", True)
        # Consumed by C++ code shared across every nordic-family variant (core.cpp, etc.).
        cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_FAMILY_NORDIC")
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
            if zephyr_variant_family() == "nordic":
                # ARM Cortex-M's ARCH_HAS_STACKWALK only defaults on when this is
                # also set (arch/arm/core/Kconfig selects the dependency it needs);
                # RISC-V (esp32_h2/c6) enables ARCH_HAS_STACKWALK unconditionally,
                # so it doesn't need this and setting it there would just warn.
                zephyr_add_prj_conf("EXTRA_EXCEPTION_INFO", True)
            zephyr_add_prj_conf("EXCEPTION_STACK_TRACE", True)

    CORE.add_job(_kconfig_options_to_code, config)
    CORE.add_job(_overlays_to_code, config)
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
async def _overlays_to_code(config: ConfigType) -> None:
    # Runs at the lowest priority so user-supplied overlay content is appended after
    # every component's own generated overlays (e.g. a partition table redefinition
    # needs to come after anything else touching &flash0).
    overlays = config.get(CONF_OVERLAYS, {})
    if app_overlay := overlays.get("app"):
        zephyr_add_overlay(app_overlay)
    if mcuboot_overlay := overlays.get("mcuboot"):
        zephyr_add_overlay(mcuboot_overlay, image="mcuboot")


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
    if isinstance(value, HexValue):
        return hex(value.value)
    if isinstance(value, int):
        return str(value)
    if isinstance(value, str):
        return f'"{value}"'
    raise ValueError


def zephyr_add_cdc_acm(config: ConfigType, id: int) -> str:
    """Ensure a CDC-ACM UART is available and return the devicetree label to
    reference it by (e.g. for `zephyr,console`/`zephyr,shell-uart`).

    Reuses the board's own `zephyr,cdc-acm-uart` node if one is already
    declared -- e.g. boards/common/usb/cdc_acm_serial.dtsi, labeled
    `board_cdc_acm_uart` -- instead of unconditionally declaring a second,
    separate USB interface with its own `cdc_acm_uart{id}` node. Two CDC-ACM
    interfaces on the same device is needless USB complexity, and means
    "which port do I talk to" changes depending on whether MCUboot (which
    only ever sees the board's own node) or the app is currently running.
    """
    framework_ver: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    if CORE.is_nrf52:
        # platform: nrf52 stays pinned to the legacy USB device stack -- untested
        # against the newer device_next stack, not touching working behavior.
        if framework_ver >= cv.Version(3, 2, 0):
            zephyr_add_prj_conf("CONFIG_USB_DEVICE_STACK_NEXT", False)
        zephyr_add_prj_conf("USB_DEVICE_STACK", True)
        zephyr_add_prj_conf("USB_CDC_ACM", True)
        # prevent device to go to susspend, without this communication stop working in python
        # there should be a way to solve it
        zephyr_add_prj_conf("USB_DEVICE_REMOTE_WAKEUP", False)
        # prevent logging when buffer is full
        zephyr_add_prj_conf("USB_CDC_ACM_LOG_LEVEL_WRN", True)
    else:
        zephyr_add_prj_conf("CONFIG_USB_DEVICE_STACK_NEXT", True)
        zephyr_add_prj_conf("CONFIG_CDC_ACM_SERIAL_INITIALIZE_AT_BOOT", True)

    from .dts_lookup import get_existing_cdc_acm_uart_label

    existing_label = get_existing_cdc_acm_uart_label(zephyr_data()[KEY_BOARD])
    if existing_label is not None:
        return existing_label

    label = f"cdc_acm_uart{id}"
    zephyr_add_overlay(
        f"""
            &zephyr_udc0 {{
                {label}: {label} {{
                    compatible = "zephyr,cdc-acm-uart";
                }};
            }};
        """
    )
    return label


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

# Like cv.SOURCE_SCHEMA's git shape, but `url` is optional: `framework: source: {ref: ...}`
# alone is enough to pin an alpha/pre-release branch of the selected framework's own
# official manifest (see resolve_framework_version(), which fills in the default).
_FRAMEWORK_SOURCE_GIT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_URL): cv.url,
        cv.Optional(CONF_REF): cv.git_ref,
        cv.Optional(CONF_USERNAME): cv.string,
        cv.Optional(CONF_PASSWORD): cv.string,
        cv.Optional(CONF_PATH): cv.string,
    }
)
_FRAMEWORK_SOURCE_SCHEMA = cv.Any(
    cv.validate_source_shorthand,
    cv.typed_schema(
        {
            TYPE_GIT: _FRAMEWORK_SOURCE_GIT_SCHEMA,
            TYPE_LOCAL: cv.LOCAL_SCHEMA,
        }
    ),
)

# `type:` isn't validated against the variant's own sdk names here -- which variant is
# selected isn't known until _ZEPHYR_SCHEMA's own CONF_VARIANT key is read, and this
# schema is applied independently of key order. resolve_framework_version() (called
# per-variant, after _ZEPHYR_SCHEMA) does that check, along with version:/source:
# mutual exclusion and everything else that needs the variant to already be known.
_FRAMEWORK_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_TYPE): cv.string_strict,
        # Also accepts the literal "recommended", an explicit alias for the selected
        # sdk's default_version (same effect as omitting this key).
        cv.Optional(CONF_VERSION): cv.string_strict,
        # Overrides the selected sdk's own official manifest with a fork/branch/ref or a
        # `local:` path to an already-initialized west workspace. `url` may be omitted to
        # target a branch (e.g. an alpha/pre-release) of the sdk's own official manifest.
        cv.Optional(CONF_SOURCE): _FRAMEWORK_SOURCE_SCHEMA,
        # How often a git source: is re-checked for updates (via `git pull`) -- own knob,
        # independent of board_source:'s (a different git source with its own cadence).
        cv.Optional(CONF_REFRESH, default="1d"): cv.All(cv.string, cv.source_refresh),
    }
)

# Own refresh: knob (see _FRAMEWORK_SCHEMA's comment) -- board_source: is a different git
# source than framework: source:, so it doesn't share a cadence with it. Unlike
# framework: source:, board_source: has no "official" default url to fall back to, so it
# keeps requiring an explicit url like the generic cv.SOURCE_SCHEMA it's otherwise
# modeled on.
_BOARD_SOURCE_GIT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_URL): cv.url,
        cv.Optional(CONF_REF): cv.git_ref,
        cv.Optional(CONF_USERNAME): cv.string,
        cv.Optional(CONF_PASSWORD): cv.string,
        cv.Optional(CONF_PATH): cv.string,
        cv.Optional(CONF_REFRESH, default="1d"): cv.All(cv.string, cv.source_refresh),
    }
)
_BOARD_SOURCE_SCHEMA = cv.Any(
    cv.validate_source_shorthand,
    cv.typed_schema(
        {
            TYPE_GIT: _BOARD_SOURCE_GIT_SCHEMA,
            TYPE_LOCAL: cv.LOCAL_SCHEMA,
        }
    ),
)
# cv.validate_source_shorthand (e.g. board_source: github://user/repo) resolves through
# the generic cv.SOURCE_SCHEMA, not the schema above, so a shorthand board_source never
# gets this refresh: default baked in -- _resolve_board_source() falls back to this
# constant for that case instead of relying on the schema to inject it.
_DEFAULT_REFRESH = cv.All(cv.string, cv.source_refresh)("1d")

# Not applied when zephyr is auto-loaded as a shared dependency (e.g. by nrf52) --
# see _variant_config_schema.
_ZEPHYR_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_VARIANT): cv.one_of(*VARIANTS, upper=True),
        cv.Optional(CONF_BOARD): cv.string_strict,
        cv.Optional(CONF_BOARD_SOURCE): _BOARD_SOURCE_SCHEMA,
        # Which SDK to build against (type:), its version, where to fetch it from
        # (source:), and how often to recheck a git source: (refresh:) -- see
        # resolve_framework_version() in variants/__init__.py for the full resolution
        # logic.
        cv.Optional(CONF_FRAMEWORK, default={}): _FRAMEWORK_SCHEMA,
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
        # Loose passthrough here -- shape differs per variant, strictly validated by
        # the variant's own config_schema.
        cv.Optional(CONF_ADVANCED, default={}): dict,
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
        # Raw passthrough to `west build -S <name>`, one per entry -- Zephyr's snippet
        # mechanism for devicetree/Kconfig fragments selected by name (e.g. Espressif's
        # own espressif-flash-4M/flash-8M/... board-variant snippets), a different layer
        # from kconfig_options: above. Lets a stock upstream board cover a differently
        # sized flash/PSRAM SKU without forking a whole custom board.
        cv.Optional(CONF_SNIPPETS, default=[]): cv.ensure_list(cv.string_strict),
        # Raw devicetree overlay passthrough -- lets a stock upstream board's DT be
        # patched (e.g. a partition table redefined for MCUboot) without forking a
        # custom board. "mcuboot" targets the sysbuild bootloader child image.
        cv.Optional(CONF_OVERLAYS, default={}): cv.Schema(
            {
                cv.Optional("app"): cv.string,
                cv.Optional("mcuboot"): cv.string,
            }
        ),
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
                refresh=conf.get(CONF_REFRESH, _DEFAULT_REFRESH),
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
    variant = config[CONF_VARIANT]
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
        # 10s: covers ZBOSS's heavy CPU usage during zigbee startup without needing a
        # separate zigbee-conditional bump (platform: nrf52's own equivalent) -- the
        # plain default was arbitrary anyway.
        config.setdefault(CONF_WATCHDOG_TIMEOUT, cv.TimePeriod(seconds=10))
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
    elif variant == ZEPHYR_VARIANT_NRF52:
        from .variants.nrf52 import config_schema as _nrf52_config_schema

        config = _nrf52_config_schema(config)
    elif variant == ZEPHYR_VARIANT_NRF54L15:
        from .variants.nrf54l15 import config_schema as _nrf54l15_config_schema

        config = _nrf54l15_config_schema(config)
    else:
        raise cv.Invalid(f"Variant {variant!r} has no config schema registered yet")
    zephyr_data()[KEY_BOARD_ROOT] = board_root
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
    sdk_name, sdk = resolve_sdk(
        VARIANTS[variant], zephyr_data().get(KEY_FRAMEWORK_TYPE)
    )
    await fetch_board_dts(
        variant,
        sdk_name,
        sdk,
        zephyr_data()["sdk_source"],
        config[CONF_FRAMEWORK][CONF_REFRESH],
    )

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
    if variant == ZEPHYR_VARIANT_NRF52:
        from .variants.nrf52 import to_code as _nrf52_to_code

        await _nrf52_to_code(config)
        return
    if variant == ZEPHYR_VARIANT_NRF54L15:
        from .variants.nrf54l15 import to_code as _nrf54l15_to_code

        await _nrf54l15_to_code(config)
        return
    raise NotImplementedError(f"Zephyr variant {variant!r} has no to_code registered")


def upload_program(config: ConfigType, args, host: str) -> bool:
    if KEY_ZEPHYR not in CORE.data:
        # `esphome upload`/`logs` can skip full config validation via the
        # validated-config cache (see compiled_config.py), so the variant
        # runtime state that _variant_config_schema() ordinarily populates as
        # a side effect never got set. Re-run it on the already-validated
        # (round-tripped) zephyr: block to repopulate it.
        zephyr_config = config.get(CORE.target_platform)
        if not zephyr_config:
            raise EsphomeError(
                "Zephyr platform configuration is missing; "
                "please re-validate and recompile."
            )
        CONFIG_SCHEMA(zephyr_config)

    if host == "PYOCD":
        if zephyr_variant_family() == "esp32":
            return False  # PYOCD isn't supported on esp32-family; it flashes over serial instead

        from .build_zephyr import run_west_flash_pyocd
        from .framework_west import check_and_install as west_install

        version = str(CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION])
        variant_data = VARIANTS[zephyr_variant()]
        _, sdk = resolve_sdk(variant_data, zephyr_data().get(KEY_FRAMEWORK_TYPE))
        python_bin, framework_path, west_env = west_install(
            sdk,
            version,
            zephyr_data()["west_version"],
            zephyr_data()["ninja_version"],
            zephyr_data()["sdk_source"],
            config[CORE.target_platform][CONF_FRAMEWORK][CONF_REFRESH],
        )
        build_dir = CORE.relative_build_path(".west_build")
        if not run_west_flash_pyocd(python_bin, framework_path, west_env, build_dir):
            raise EsphomeError("Zephyr pyocd flash failed")
        return True

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
    _, sdk = resolve_sdk(variant_data, zephyr_data().get(KEY_FRAMEWORK_TYPE))
    python_bin, framework_path, west_env = west_install(
        sdk,
        version,
        zephyr_data()["west_version"],
        zephyr_data()["ninja_version"],
        zephyr_data()["sdk_source"],
        config[CORE.target_platform][CONF_FRAMEWORK][CONF_REFRESH],
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

    from .build_zephyr import generate_cmake_lists, run_west_blobs_fetch, run_west_build
    from .framework_west import check_and_install as west_install
    from .sdk_setup_west import check_and_install as sdk_install

    variant_data = VARIANTS[variant]
    _, sdk = resolve_sdk(variant_data, zephyr_data().get(KEY_FRAMEWORK_TYPE))
    version = str(CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION])
    python_bin, framework_path, west_env = west_install(
        sdk,
        version,
        zephyr_data()["west_version"],
        zephyr_data()["ninja_version"],
        zephyr_data()["sdk_source"],
        config[CORE.target_platform][CONF_FRAMEWORK][CONF_REFRESH],
    )
    cross_toolchain = variant_data.toolchain
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
        snippets=zephyr_data()[KEY_SNIPPETS],
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
