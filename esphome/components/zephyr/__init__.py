from collections.abc import Callable
import logging
import os
from pathlib import Path
import re
import subprocess
import textwrap
from typing import TypedDict

import yaml

from esphome import git
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADVANCED,
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_LOG_LEVEL,
    CONF_NAME,
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

from .board_revision import declared_revisions, parse_board_string, resolve_revision
from .const import (
    BOOTLOADER_MCUBOOT,
    CONF_BOARD_SOURCE,
    CONF_CDC_ACM,
    CONF_KCONFIG_OPTIONS,
    CONF_MODULES,
    CONF_NINJA_VERSION,
    CONF_OVERLAYS,
    CONF_SHIELD_SOURCE,
    CONF_SHIELDS,
    CONF_SINGLE_SLOT,
    CONF_SNIPPET_SOURCE,
    CONF_SNIPPETS,
    CONF_WEST_VERSION,
    KEY_BOARD,
    KEY_BOARD_ROOT,
    KEY_BOOTLOADER,
    KEY_EXTRA_BUILD_FILES,
    KEY_FRAMEWORK_TYPE,
    KEY_KCONFIG,
    KEY_MODULE_OVERRIDES,
    KEY_MODULE_REQUESTS,
    KEY_OVERLAY,
    KEY_OVERLAY_BUILDER,
    KEY_PM_STATIC,
    KEY_PRJ_CONF,
    KEY_RUNNER,
    KEY_SHIELD_ROOT,
    KEY_SHIELDS,
    KEY_SINGLE_SLOT,
    KEY_SNIPPET_ROOT,
    KEY_SNIPPETS,
    KEY_SYSBUILD_CONF,
    KEY_USER,
    KEY_ZEPHYR,
    ZEPHYR_VARIANT_EFR32MG24,
    ZEPHYR_VARIANT_ESP32,
    ZEPHYR_VARIANT_ESP32_C3,
    ZEPHYR_VARIANT_ESP32_C5,
    ZEPHYR_VARIANT_ESP32_C6,
    ZEPHYR_VARIANT_ESP32_H2,
    ZEPHYR_VARIANT_NATIVE_SIM,
    ZEPHYR_VARIANT_NRF52,
    ZEPHYR_VARIANT_NRF54L15,
    ZEPHYR_VARIANT_NRF54LM20A,
    ZEPHYR_VARIANT_RA4M1,
    ZEPHYR_VARIANT_RP2040,
    ZEPHYR_VARIANT_RP2350,
    ZEPHYR_VARIANT_STM32F1,
    ZEPHYR_VARIANT_STM32F4,
    ZEPHYR_VARIANT_STM32L4,
    ZEPHYR_VARIANT_STM32U5,
    ZEPHYR_VARIANT_STM32WB55,
    zephyr_ns,
)
from .gpio import zephyr_pin_to_code as _zephyr_pin_to_code  # noqa: F401
from .variants import VARIANTS, ZephyrModule, ZephyrModuleTemplate, resolve_sdk

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
    def __init__(self, name: str, address: int, size: int, region: str) -> None:
        self.name = name
        self.address = address
        self.size = size
        self.region = region
        self.end_address = self.address + self.size

    def __str__(self) -> str:
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
    family: (
        str | None
    )  # variant's ZephyrVariant.family (e.g. "esp32"), or None if unfamilied
    framework_type: str  # resolved zephyr: framework: type: (e.g. "zephyr", "ncs")
    prj_conf: dict[str, dict[str, tuple[PrjConfValueType, bool]]]
    sysbuild_conf: dict[str, tuple[PrjConfValueType, bool]]
    overlay: dict[str, str]
    extra_build_files: dict[str, Path]
    pm_static: list[Section]
    user: dict[str, list[str]]
    kconfig: str
    overlay_builder: list[Callable[[], str]]
    fake_board_manifest: str | None
    dts_base_path: (
        str | None
    )  # local path to zephyr/ tree root with boards/; set by dts_fetch
    i2c_bus_cache: dict[
        str, object
    ]  # board -> list[str] | _NOT_FOUND; cleared each run
    spi_bus_cache: dict[
        str, object
    ]  # board -> list[str] | _NOT_FOUND; cleared each run
    cpp_path: str | None  # "" = unchecked; None = not found; else = executable path
    board_dir_cache: dict[str, str]  # board -> abs path str, "" = not found
    dts_include_paths: list[str] | None  # None = not yet computed
    # (board, shields, snippets) -> EDT | _NOT_FOUND; cleared each run. Value stays
    # `object` since edtlib.EDT isn't imported here (dts_lookup.py's own concern).
    board_edt_cache: dict[tuple[str, tuple[str, ...], tuple[str, ...]], object]
    board_yaml_cache: dict[
        str, object
    ]  # board -> list[str] | _NOT_FOUND; cleared each run
    west_version: str | None  # None = use requirements_west.txt's pinned version
    ninja_version: str | None  # None = use requirements_west.txt's pinned version
    snippets: list[
        str
    ]  # zephyr: snippets: -- one `-S <name>` per entry to `west build`
    swap_method: str | None  # ota: swap_method:, set by mcuboot.apply_swap_method()
    single_slot: bool  # zephyr: single_slot: -- see mcuboot.apply_single_slot()
    shields: list[str]  # zephyr: shields: -- one `-DSHIELD=` entry per item
    shield_root: (
        Path | None
    )  # resolved zephyr: shield_source: dir, or board_root, or None
    snippet_root: (
        Path | None
    )  # resolved zephyr: snippet_source: dir, or board_root, or None
    # capability -> template, requested via request_zephyr_module(). Not yet resolved
    # to a version -- a zephyr: modules: override (module_overrides) may still apply
    # before final resolution in run_compile(), so resolving eagerly here could raise
    # for a version gap the user's own override was going to fill.
    module_requests: dict[str, ZephyrModuleTemplate]
    # module name -> a full user-authored ZephyrModule (zephyr: modules: with source:),
    # or a plain version string overriding a component-requested module's default.
    module_overrides: dict[str, ZephyrModule | str]
    # (west_module, allow_regex, sentinel_name) entries added via zephyr_add_blobs() --
    # transport-conditional blob fetches, on top of the variant's own static `blobs`.
    blobs: list[tuple[str, str, str]]
    runner: str | None  # zephyr: advanced: runner: -- west flash runner override


# platform: nrf52 use only
def zephyr_set_core_data(config: ConfigType) -> None:
    CORE.data[KEY_ZEPHYR] = ZephyrData(
        board=config[CONF_BOARD],
        board_root=None,
        sdk_source=None,
        bootloader=config[KEY_BOOTLOADER],
        variant=None,
        family=None,
        # platform: nrf52 is inherently NCS-based (no alternate).
        framework_type="ncs",
        prj_conf={},
        sysbuild_conf={},
        overlay={
            "": "",
        },  # set empty to make sure that overlay is cleared after config change
        overlay_builder=[],
        extra_build_files={},
        pm_static=[],
        user={},
        kconfig="",
        fake_board_manifest=None,
        dts_base_path=None,
        i2c_bus_cache={},
        spi_bus_cache={},
        cpp_path="",
        board_dir_cache={},
        dts_include_paths=None,
        board_edt_cache={},
        board_yaml_cache={},
        west_version=None,
        ninja_version=None,
        snippets=[],
        swap_method=None,
        single_slot=False,
        shields=[],
        shield_root=None,
        snippet_root=None,
        module_requests={},
        module_overrides={},
        blobs=[],
        runner=None,
    )


def zephyr_data() -> ZephyrData:
    return CORE.data[KEY_ZEPHYR]


def zephyr_variant() -> str | None:
    """Return the active Zephyr variant name, or None if not yet set."""
    return zephyr_data().get("variant")


def zephyr_framework_type() -> str | None:
    """Return the active `zephyr: framework: type:` (e.g. "zephyr", "ncs"), or None if
    not yet set."""
    return zephyr_data().get(KEY_FRAMEWORK_TYPE)


def zephyr_variant_family() -> str | None:
    """Return the active variant's silicon family (e.g. "esp32"), or None if unset/unfamilied."""
    return zephyr_data().get("family")


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
    elif zephyr_variant_family() == "silabs":
        # Same reasoning as esp32 above: override the board's fixed i2c*_default
        # pinctrl node so the pins actually used match the user's sda:/scl: YAML.
        # Silicon Labs' pinctrl macros are lettered-port form ({BUS}_{SIGNAL}_P{port}{n},
        # e.g. I2C0_SDA_PC5) rather than ESP32's flat GPIO{n} form.
        prefix = bus_label.upper()
        port_width = VARIANTS[variant_name].gpio_port_width
        sda_letter, sda_pin = chr(ord("A") + sda // port_width), sda % port_width
        scl_letter, scl_pin = chr(ord("A") + scl // port_width), scl % port_width
        zephyr_add_overlay(
            f"""
                &pinctrl {{
                    {bus_label}_default: {bus_label}_default {{
                        group0 {{
                            pins = <{prefix}_SCL_P{scl_letter}{scl_pin}>,
                                <{prefix}_SDA_P{sda_letter}{sda_pin}>;
                            bias-pull-up;
                            drive-open-drain;
                        }};
                    }};
                }};
            """
        )
    # Other variants: pinctrl already defined in board DTS; no overlay needed.

    return sda, scl


# Families whose boards ship SPI with fixed, already-wired pinctrl in the board DTS --
# the clk/mosi/miso pins in YAML must match that fixed wiring, only the bus itself
# needs enabling. Verified against real board DTS (v4.4.1): nucleo_f401re (stm32) has
# &spi1 with pinctrl-0 already set; xiao_ra4m1/ek_ra4m1 (renesas) likewise for &spi1.
# esp32's GPIO matrix is free-mux and handled separately below. Other free-mux
# families (nordic's SPIM, silabs) and rp2040 (whose boards ship no SPI pinctrl at
# all -- would need a real overlay, not just enabling) still need a generated
# pinctrl overlay the way zephyr_setup_i2c_pinctrl() does for I2C -- not implemented
# yet, so those families are rejected explicitly rather than silently binding to
# whatever pins the board happens to default to.
_SPI_FIXED_PINCTRL_FAMILIES = frozenset({"stm32", "renesas"})

# esp32-family SPI instance -> GPIO-matrix signal prefix, verified against real
# per-chip devicetree source (v4.4.1): base ESP32 has two general-purpose SPI
# controllers, spi2=HSPI/spi3=VSPI (esp32_common.dtsi's
# `clocks = <&clock ESP32_HSPI_MODULE>`/`ESP32_VSPI_MODULE`). C3/C5/C6/H2 each have
# only a single one, spi2=FSPI (confirmed via each chip's own -gpio-sigmap.h; no
# spi3 node exists on any of them). S2/S3/P4 are not supported variants in this repo
# yet and are deliberately not included here.
_ESP32_SPI_INSTANCE_SIGNAL_PREFIX = {
    ZEPHYR_VARIANT_ESP32: {2: "HSPI", 3: "VSPI"},
    ZEPHYR_VARIANT_ESP32_C3: {2: "FSPI"},
    ZEPHYR_VARIANT_ESP32_C5: {2: "FSPI"},
    ZEPHYR_VARIANT_ESP32_C6: {2: "FSPI"},
    ZEPHYR_VARIANT_ESP32_H2: {2: "FSPI"},
}


def _esp32_spi_pinctrl_overlay(
    bus_label: str,
    clk: int,
    miso: int | None,
    mosi: int | None,
    data_pins: list[int] | None,
) -> str:
    """Build a pinctrl overlay for esp32's free-mux SPI GPIO matrix.

    Uses the auto-generated `SPIM{n}_{SIGNAL}_GPIO{n}` convenience macros
    (`<variant>-pinctrl.h`) for CLK/MISO/MOSI -- same shape as
    zephyr_setup_i2c_pinctrl()'s esp32 branch. Those macros don't exist for
    quad's HD/WP lines (verified: no SPIM{n}_HD_GPIO*/WP_GPIO* macros in any
    in-scope chip's pinctrl header), so those two are hand-built from the raw
    ESP32_PINMUX() signal IDs instead.
    """
    instance = int(bus_label.removeprefix("spi"))
    prefix = _ESP32_SPI_INSTANCE_SIGNAL_PREFIX[zephyr_variant()][instance]
    macro_prefix = f"SPIM{instance}"

    if data_pins:
        # Quad mode routes mosi/miso through data_pins[0]/[1] (D0/D1) instead of
        # separate mosi_pin/miso_pin -- schema forbids the latter for quad.
        mosi, miso = data_pins[0], data_pins[1]

    pinmux = [f"<{macro_prefix}_SCLK_GPIO{clk}>"]
    if miso is not None:
        pinmux.append(f"<{macro_prefix}_MISO_GPIO{miso}>")
    if mosi is not None:
        pinmux.append(f"<{macro_prefix}_MOSI_GPIO{mosi}>")

    groups = f"""
        group1 {{
            pinmux = {", ".join(pinmux)};
        }};
    """
    if data_pins:
        # data_pins[2]/[3] are WP/HD (data_pins[0]/[1] are the mosi/miso-role D0/D1
        # lines, already routed above via mosi/miso) -- same order as ESP-IDF's own
        # data0_io_num.._data3_io_num (spi_esp_idf.cpp).
        wp, hd = data_pins[2], data_pins[3]
        groups += f"""
            group2 {{
                pinmux = <ESP32_PINMUX({wp}, ESP_NOSIG, ESP_{prefix}WP_OUT)>,
                    <ESP32_PINMUX({hd}, ESP_NOSIG, ESP_{prefix}HD_OUT)>;
            }};
        """

    return f"""
        &pinctrl {{
            {bus_label}_default: {bus_label}_default {{
                {groups}
            }};
        }};
    """


def zephyr_setup_spi_pinctrl(
    board: str,
    bus_label: str,
    clk: int | None = None,
    miso: int | None = None,
    mosi: int | None = None,
    data_pins: list[int] | None = None,
) -> None:
    """Enable the hardware SPI bus node for `bus_label`.

    Families in _SPI_FIXED_PINCTRL_FAMILIES ship pinctrl fixed in their own board
    DTS -- only the bus itself needs enabling, clk/miso/mosi/data_pins are unused.
    esp32's GPIO matrix has no fixed pinctrl, so clk/miso/mosi/data_pins are used to
    generate a pinctrl overlay instead (mirrors zephyr_setup_i2c_pinctrl()'s esp32
    branch). Other free-mux families (nordic, silabs, rp2040) aren't implemented yet.
    """
    family = zephyr_variant_family()
    if family in _SPI_FIXED_PINCTRL_FAMILIES:
        zephyr_add_overlay(f'&{bus_label} {{ status = "okay"; }};')
        return

    if family != "esp32":
        raise cv.Invalid(
            f"Hardware SPI on Zephyr variant family '{family}' is not implemented yet "
            "(needs a generated pinctrl overlay for its free pin muxing). "
            "Use 'interface: software' instead."
        )

    if clk is None:
        raise cv.Invalid("Could not determine SPI pin assignments for this board.")

    valid_instances = _ESP32_SPI_INSTANCE_SIGNAL_PREFIX.get(zephyr_variant(), {})
    instance = int(bus_label.removeprefix("spi")) if bus_label[3:].isdigit() else None
    if instance not in valid_instances:
        listed = ", ".join(f"spi{n}" for n in sorted(valid_instances))
        raise cv.Invalid(
            f"'{bus_label}' is not a valid SPI bus for Zephyr variant "
            f"{zephyr_variant()!r} -- valid options: {listed}"
        )

    overlay = _esp32_spi_pinctrl_overlay(bus_label, clk, miso, mosi, data_pins)
    overlay += f"""
        &{bus_label} {{
            status = "okay";
            pinctrl-0 = <&{bus_label}_default>;
            pinctrl-names = "default";
        }};
    """
    zephyr_add_overlay(overlay)
    if data_pins:
        zephyr_add_prj_conf("SPI_EXTENDED_MODES", True)


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


def zephyr_set_sysbuild_conf_override(name: str, value: PrjConfValueType) -> None:
    """Unconditionally set a sysbuild-level Kconfig value (SB_CONFIG_*), replacing
    anything set by a component's own zephyr_add_sysbuild_conf() call.

    Same always-wins precedent as zephyr_set_prj_conf_override(), but for
    zephyr/sysbuild.conf (SB_CONFIG_*, e.g. a vendor SDK's own `choice` default in a
    Kconfig.sysbuild file) rather than an image's own CONFIG_* prj.conf -- a user-supplied
    `kconfig_options:` name is routed here automatically when it already starts with
    SB_CONFIG_.
    """
    zephyr_data()[KEY_SYSBUILD_CONF][name] = (value, True)


def request_zephyr_module(capability: str) -> None:
    """Component-facing: record that `capability` (e.g. "zigbee") is needed as an
    additive west module. Not resolved to a concrete version here -- a user's
    `zephyr: modules:` override may still apply, so failing on a missing default
    version has to wait until resolve_zephyr_modules() (see there for why)."""
    _, sdk = resolve_sdk(VARIANTS[zephyr_variant()], zephyr_framework_type())
    template = sdk.modules.get(capability)
    if template is None:
        raise EsphomeError(
            f"The '{capability}' module isn't available for "
            f"framework: type: {zephyr_framework_type()}."
        )
    requests = zephyr_data()[KEY_MODULE_REQUESTS]
    if (existing := requests.get(capability)) is not None and existing != template:
        raise EsphomeError(
            f"The '{capability}' module was already requested with a different source"
        )
    requests[capability] = template


def zephyr_set_module_override(name: str, override: ZephyrModule | str) -> None:
    """Unconditionally record a `zephyr: modules:` override, replacing anything a
    component requested. `override` is a full ZephyrModule (source: given -- a
    brand-new or replaced module) or a plain version string (version-only tweak of a
    component-requested module). Always wins -- same precedent as
    zephyr_set_prj_conf_override for kconfig_options:.
    """
    zephyr_data()[KEY_MODULE_OVERRIDES][name] = override


def resolve_zephyr_modules() -> list[ZephyrModule]:
    """Combine module_requests with any module_overrides into the final set of
    modules to fetch. Called once, from run_compile() -- by then every component's
    to_code() and the zephyr: modules: override job have both already run, so an
    override is guaranteed visible before a request's version gets resolved."""
    root_version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    overrides = dict(zephyr_data()[KEY_MODULE_OVERRIDES])
    resolved: dict[str, ZephyrModule] = {}
    seen_templates: dict[str, ZephyrModuleTemplate] = {}
    for capability, template in zephyr_data()[KEY_MODULE_REQUESTS].items():
        if (
            existing := seen_templates.get(template.name)
        ) is not None and existing != template:
            raise EsphomeError(
                f"Two different module requests both resolve to '{template.name}' "
                f"(most recently '{capability}') -- capabilities must map to "
                "distinct modules"
            )
        seen_templates[template.name] = template
        override = overrides.pop(template.name, None)
        if isinstance(override, ZephyrModule):
            resolved[template.name] = override
        else:
            resolved[template.name] = template.resolve(root_version, override)
    for name, override in overrides.items():
        if not isinstance(override, ZephyrModule):
            hint = ""
            if requested_names := sorted(t.name for t in seen_templates.values()):
                hint = (
                    " -- note this must be the module's own name, not a component's "
                    f"capability name (currently requested: {', '.join(requested_names)})"
                )
            raise EsphomeError(
                f"zephyr: modules: '{name}' has no source and wasn't requested by "
                f"any component{hint}"
            )
        resolved[name] = override
    return list(resolved.values())


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
    else:
        # A chunk without a trailing newline (e.g. a bare #include line) would
        # otherwise merge onto the same line as the next chunk.
        data[KEY_OVERLAY][image] += "\n"
    data[KEY_OVERLAY][image] += textwrap.dedent(content)


def zephyr_add_blobs(module: str, allow_regex: str, sentinel_name: str) -> None:
    """Request a `west blobs fetch`, gated on a transport actually being configured
    (unlike a variant's static `blobs`, which always runs)."""
    spec = (module, allow_regex, sentinel_name)
    if spec not in zephyr_data()["blobs"]:
        zephyr_data()["blobs"].append(spec)


def zephyr_add_overlay_builder(func: Callable[[], str]) -> None:
    data = zephyr_data()
    if func not in data[KEY_OVERLAY_BUILDER]:
        data[KEY_OVERLAY_BUILDER].append(func)


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
    # i2c_emulator.cpp/.h and uart_emulator.cpp/.h are auto-discovered for every Zephyr
    # build, but only make sense when the corresponding `emulation:` config is present
    # (CONFIG_I2C_EMUL / CONFIG_UART_EMUL). Without this filter, sysbuild's
    # --whole-archive link force-pulls their registration/callback functions even when
    # unused, breaking the link for real-hardware builds.
    excluded = []
    extra_build_files = zephyr_data()[KEY_EXTRA_BUILD_FILES]
    if "i2c_emulator.cpp" not in extra_build_files:
        excluded += ["i2c_emulator.cpp", "i2c_emulator.h"]
    if "uart_emulator.cpp" not in extra_build_files:
        excluded += ["uart_emulator.cpp", "uart_emulator.h"]
    return excluded


FILTER_SOURCE_FILES = _filter_source_files


def zephyr_to_code(config: ConfigType) -> None:
    cg.add_build_flag("-DUSE_ZEPHYR")
    cg.add_define("USE_NATIVE_64BIT_TIME")
    # The settings subsystem finds stored preferences by key, so key migration is possible
    cg.add_define("USE_PREFERENCE_KEY_LOOKUP")
    cg.set_cpp_standard("gnu++20")
    # platform: nrf52 has no user-facing `framework: type:` -- its internal
    # framework_type="ncs" (see zephyr_set_core_data) is Python-only, never turned into
    # a C++ define here; all C++ code keys off USE_NRF52 instead.
    # USE_ZEPHYR_FRAMEWORK_ZEPHYR not defined, no need in code so far.
    # USE_ZEPHYR_FRAMEWORK_ZIGBEE is set by zigbee_zephyr.py itself, alongside its
    # request_zephyr_module("zigbee") call -- zephyr_to_code() here runs before
    # zigbee:'s own to_code() in the normal dependency order, so the module request
    # wouldn't be visible yet if it were decided here instead.
    if zephyr_variant() is not None and zephyr_framework_type() == "ncs":
        cg.add_define("USE_ZEPHYR_FRAMEWORK_NCS")
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
    elif zephyr_variant_family() == "silabs":
        # Same reasoning as esp32/nordic above: mainline Zephyr's MINIMAL_LIBCPP has no
        # STL, which ESPHome's C++ core requires regardless of chip vendor.
        zephyr_add_prj_conf("CPP", True)
        zephyr_add_prj_conf("REQUIRES_FULL_LIBCPP", True)
        # Consumed by C++ code shared across every silabs-family variant (core.cpp, etc.).
        cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_FAMILY_SILABS")
    elif zephyr_variant_family() == "stm32":
        zephyr_add_prj_conf("CPP", True)
        zephyr_add_prj_conf("REQUIRES_FULL_LIBCPP", True)
        cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_FAMILY_STM32")
    elif zephyr_variant_family() == "renesas":
        # Same reasoning as esp32/nordic/silabs above: mainline Zephyr's MINIMAL_LIBCPP
        # has no STL, which ESPHome's C++ core requires regardless of chip vendor.
        zephyr_add_prj_conf("CPP", True)
        zephyr_add_prj_conf("REQUIRES_FULL_LIBCPP", True)
        # Consumed by C++ code shared across every renesas-family variant (core.cpp, etc.).
        cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_FAMILY_RENESAS")
    elif zephyr_variant_family() == "rpi_pico":
        # Same reasoning as esp32/nordic/silabs above: mainline Zephyr's MINIMAL_LIBCPP
        # has no STL, which ESPHome's C++ core requires regardless of chip vendor.
        zephyr_add_prj_conf("CPP", True)
        zephyr_add_prj_conf("REQUIRES_FULL_LIBCPP", True)
        # Consumed by C++ code shared across every rpi_pico-family variant (core.cpp, etc.).
        cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_FAMILY_RPI_PICO")
        # Lets logger_zephyr.cpp's USB_CDC poll loop detect a host opening the port at
        # 1200 baud (the cross-ecosystem "magic baud rate" convention) and reboot into
        # the ROM USB bootloader (BOOTSEL) without needing the physical button --
        # backed entirely by already-merged Zephyr infrastructure (subsys/retention's
        # bootmode API + the RP2 SoC's own PRE_KERNEL_2 hook that acts on it).
        #
        # Applied directly rather than via the upstream `rp2-boot-mode-retention`
        # snippet: that snippet's board-matching regex (`.*/rp2350b?/.*`) doesn't
        # account for the "A package" qualifier real RP2350 boards use
        # (`xiao_rp2350/rp2350a/m33`), so its devicetree overlay silently never
        # applies there, and CONFIG_RETENTION_BOOT_MODE gets silently dropped for
        # lack of the "zephyr,boot-mode" chosen node it depends on.
        boot_mode_dtsi = (
            "rp2040-boot-mode-retention"
            if zephyr_variant() == ZEPHYR_VARIANT_RP2040
            else "rp2350-boot-mode-retention"
        )
        boot_mode_overlay = f"#include <vendor/raspberrypi/{boot_mode_dtsi}.dtsi>"
        # When mcuboot is enabled, it -- not the app -- is what runs first on the next
        # boot after the app calls bootmode_set()+sys_reboot(). The RP2 SoC's own
        # PRE_KERNEL_2 hook that reads the retained flag and jumps into the ROM USB
        # bootloader has to run inside mcuboot's own boot sequence, since mcuboot
        # decides whether to chain-load the app at all -- so mcuboot's own build needs
        # this Kconfig/overlay applied too (image="mcuboot"), or nothing ever reads the
        # flag and mcuboot just boots the app slot as normal.
        images = ("",)
        if zephyr_data()[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT:
            images = ("", "mcuboot")
        for image in images:
            zephyr_add_prj_conf("RETAINED_MEM", True, image=image)
            zephyr_add_prj_conf("RETENTION", True, image=image)
            zephyr_add_prj_conf("RETENTION_BOOT_MODE", True, image=image)
            zephyr_add_overlay(boot_mode_overlay, image)
        cg.add_build_flag("-DUSE_ZEPHYR_BOOTSEL_TOUCH")
    else:
        # No zephyr variant: platform: nrf52 calling this shared helper directly, uses newlib.
        zephyr_add_prj_conf("NEWLIB_LIBC", True)
        zephyr_add_prj_conf("NEWLIB_LIBC_FLOAT_PRINTF", True)

    if zephyr_data()[KEY_SINGLE_SLOT]:
        from . import mcuboot  # noqa: PLC0415

        mcuboot.apply_single_slot()

    # esp32_h2/esp32_c6/esp32_c5 are RV32IMAC and esp32_c3 is RV32IMC -- none have a
    # hardware FPU; rp2040 is Cortex-M0+, also without FPU. Original ESP32 is Xtensa LX6,
    # which does have one -- it can't be excluded by family the way these chips are.
    if zephyr_variant() not in (
        ZEPHYR_VARIANT_ESP32_H2,
        ZEPHYR_VARIANT_ESP32_C6,
        ZEPHYR_VARIANT_ESP32_C5,
        ZEPHYR_VARIANT_ESP32_C3,
        ZEPHYR_VARIANT_RP2040,
    ):
        zephyr_add_prj_conf("FPU", True)
    zephyr_add_prj_conf("STD_CPP20", True)
    # random_bytes() uses sys_rand_get() which requires the entropy subsystem. RP2040 has
    # no hardware RNG; RP2350's does exist but Zephyr's driver for it hangs the whole boot
    # sequence (unbounded busy-wait, no timeout -- see rp2350.py's TEST_RANDOM_GENERATOR).
    # STM32F4 is a whole chip family, not a single SoC -- RNG presence varies per member
    # (F401/F411 have none, F405/F410/F412 and larger do), so stm32f4.py resolves this
    # itself from the board's own DTS instead of a blanket per-variant default. STM32F1
    # has no true RNG on any family member (dts/arm/st/f1 has no rng@ node at all), so
    # stm32f1.py always uses TEST_RANDOM_GENERATOR unconditionally.
    if zephyr_variant() not in (
        ZEPHYR_VARIANT_RP2040,
        ZEPHYR_VARIANT_RP2350,
        ZEPHYR_VARIANT_STM32F4,
        ZEPHYR_VARIANT_STM32F1,
    ):
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
        timeout_ms = int(config[CONF_WATCHDOG_TIMEOUT].total_milliseconds)
        # 0 disables the watchdog: leave CONFIG_WATCHDOG unset so hal.cpp's #ifdef
        # compiles it out entirely, rather than requesting a timeout of zero.
        if timeout_ms != 0:
            zephyr_add_prj_conf("WATCHDOG", True)
            zephyr_add_prj_conf("WDT_DISABLE_AT_BOOT", False)
            cg.add_define("USE_ZEPHYR_WATCHDOG_TIMEOUT_MS", timeout_ms)
            # Every STM32 family member's iwdg node (dts/arm/st/*/stm32*.dtsi, "st,stm32-watchdog")
            # ships status = "disabled" -- CONFIG_WATCHDOG alone doesn't enable it. hal.cpp
            # resolves the watchdog device via DT_ALIAS(watchdog0), same as every other
            # family's stock board -- but unlike nrf52/esp32 boards, STM32 boards don't
            # define that alias themselves (watchdog wasn't previously used on this family),
            # so it has to be added here too, not just the node's status.
            if zephyr_variant_family() == "stm32":
                zephyr_add_overlay(
                    '&iwdg { status = "okay"; };\n'
                    "/ { aliases { watchdog0 = &iwdg; }; };"
                )
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
            if zephyr_variant_family() in (
                "nordic",
                "silabs",
                "rpi_pico",
                "stm32",
                "renesas",
            ):
                # ARM Cortex-M's ARCH_HAS_STACKWALK only defaults on when this is
                # also set (arch/arm/core/Kconfig selects the dependency it needs);
                # RISC-V (esp32_h2/c6) enables ARCH_HAS_STACKWALK unconditionally,
                # so it doesn't need this and setting it there would just warn.
                # silabs (EFR32MG24, Cortex-M33), stm32 (STM32L4, Cortex-M4), and
                # renesas (RA4M1, Cortex-M4) need the same treatment as nordic.
                zephyr_add_prj_conf("EXTRA_EXCEPTION_INFO", True)
            zephyr_add_prj_conf("EXCEPTION_STACK_TRACE", True)

    CORE.add_job(_kconfig_options_to_code, config)
    CORE.add_job(_modules_to_code, config)
    CORE.add_job(_overlays_to_code, config)
    CORE.add_job(_cdc_acm_to_code, config)


@coroutine_with_priority(CoroPriority.FINAL)
async def _modules_to_code(config: ConfigType) -> None:
    # .get(): nrf52's config dict has no modules key.
    #
    # Runs at the lowest priority so every component has already requested its own
    # modules -- a user-supplied zephyr: modules: entry must always win, and a
    # version-only override needs every request already recorded to have something to
    # attach to (final resolution happens later still, in resolve_zephyr_modules()).
    for module_conf in config.get(CONF_MODULES, []):
        name = module_conf[CONF_NAME]
        source = module_conf.get(CONF_SOURCE)
        if source is None:
            zephyr_set_module_override(name, module_conf[CONF_VERSION])
        elif source[CONF_TYPE] == TYPE_LOCAL:
            # cv.LOCAL_SCHEMA's path: already went through cv.directory -- an
            # already-resolved absolute Path, not a raw string to resolve again.
            zephyr_set_module_override(
                name, ZephyrModule(name=name, local_path=source[CONF_PATH])
            )
        else:
            zephyr_set_module_override(
                name,
                ZephyrModule(
                    name=name, manifest_url=source[CONF_URL], revision=source[CONF_REF]
                ),
            )


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
        elif name.startswith("SB_CONFIG_"):
            zephyr_set_sysbuild_conf_override(name, value)
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


def zephyr_setup_preferences() -> None:
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
    # Idempotent: zephyr_add_overlay_builder() only appends a given func once, so every
    # caller registering the same _render_user_overlay reference collapses to one deferred
    # render covering every key -- see zephyr_add_overlay_builder's copy_files() consumer.
    zephyr_add_overlay_builder(_render_user_overlay)


def _render_user_overlay() -> str:
    user = zephyr_data()[KEY_USER]
    if not user:
        return ""
    entries = " ".join(f"{key} = {', '.join(value)};" for key, value in user.items())
    return f"""
        / {{
            zephyr,user {{
                {entries}
            }};
        }};
    """


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

    for builder_func in zephyr_data()[KEY_OVERLAY_BUILDER]:
        overlay_contents = builder_func()
        zephyr_add_overlay(overlay_contents)

    if manifest := zephyr_data()["fake_board_manifest"]:
        changed |= write_file_if_changed(
            CORE.relative_build_path(f"boards/{zephyr_data()[KEY_BOARD]}.json"),
            manifest,
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
            if (
                image == "mcuboot"
                and zephyr_data()[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT
            ):
                # Writing any overlay file for the mcuboot sysbuild image replaces its
                # normal auto-detected overlay chain (Zephyr sysbuild convention),
                # which silently drops MCUboot's own
                # zephyr,code-partition = &boot_partition; setting -- restate it here,
                # once, for every caller that writes to this image's overlay, rather
                # than relying on each one to remember to.
                content += (
                    "\n/ { chosen { zephyr,code-partition = &boot_partition; }; };\n"
                )
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
# confirmed working on real ESP32-H2 hardware under Zephyr. 0 is a special case, not part of
# that shared range: it disables the watchdog entirely (no CONFIG_WATCHDOG, no wdt_setup()
# call at all -- see the zephyr_variant() watchdog codegen block below), rather than
# requesting an unusably short real timeout.
def _validate_watchdog_timeout(value):
    # Bare 0 has no unit, so it wouldn't parse as a normal TimePeriod otherwise.
    # Only the 60s ceiling is checked here; the 5s floor is variant-aware (some
    # variants can't reach it) and enforced later once VARIANTS[variant] is known.
    if isinstance(value, (int, float)) and value == 0:
        return cv.TimePeriod(seconds=0)
    period = cv.positive_time_period_seconds(value)
    if period == cv.TimePeriod(seconds=0):
        return period
    return cv.Range(max=cv.TimePeriod(seconds=60))(period)


_WATCHDOG_TIMEOUT_VALIDATOR = _validate_watchdog_timeout

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

# shield_source: is only needed when a shield lives outside board_source:'s repo (the
# uncommon case) -- same git/local shape as board_source:, so reuse its git schema
# rather than duplicating it.
_SHIELD_SOURCE_SCHEMA = cv.Any(
    cv.validate_source_shorthand,
    cv.typed_schema(
        {
            TYPE_GIT: _BOARD_SOURCE_GIT_SCHEMA,
            TYPE_LOCAL: cv.LOCAL_SCHEMA,
        }
    ),
)

# snippet_source: mirrors shield_source: -- only needed when a snippet lives outside
# board_source:'s repo (the uncommon case); same git/local shape.
_SNIPPET_SOURCE_SCHEMA = cv.Any(
    cv.validate_source_shorthand,
    cv.typed_schema(
        {
            TYPE_GIT: _BOARD_SOURCE_GIT_SCHEMA,
            TYPE_LOCAL: cv.LOCAL_SCHEMA,
        }
    ),
)

# A user-authored additive west module -- no username:/password:/refresh: (unlike
# board_source:/shield_source:/snippet_source:, these are fetched via `west update`
# as part of the generated manifest, not ESPHome's own git.clone_or_update cache).
_MODULE_GIT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_URL): cv.url,
        cv.Optional(CONF_REF, default="main"): cv.git_ref,
    }
)
_MODULE_SOURCE_SCHEMA = cv.typed_schema(
    {
        TYPE_GIT: _MODULE_GIT_SCHEMA,
        TYPE_LOCAL: cv.LOCAL_SCHEMA,
    }
)
# source: defines a brand-new (or replaced) module; version: alone is a version-only
# tweak of a module a component already requested -- see resolve_zephyr_modules(). At
# least one of the two is required, or the entry does nothing.
_MODULE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_NAME): cv.string_strict,
            cv.Optional(CONF_SOURCE): _MODULE_SOURCE_SCHEMA,
            cv.Optional(CONF_VERSION): cv.string_strict,
        }
    ),
    cv.has_at_least_one_key(CONF_SOURCE, CONF_VERSION),
)

# Not applied when zephyr is auto-loaded as a shared dependency (e.g. by nrf52) --
# see _variant_config_schema.
_ZEPHYR_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_VARIANT): cv.one_of(*VARIANTS, upper=True),
        cv.Optional(CONF_BOARD): cv.string_strict,
        cv.Optional(CONF_BOARD_SOURCE): _BOARD_SOURCE_SCHEMA,
        # Zephyr shields (physical add-on boards, e.g. an nRF7002 Wi-Fi/Zigbee
        # companion board) -- one `-DSHIELD=` entry per item, natively space/semicolon
        # stackable, hence a list rather than a single value.
        cv.Optional(CONF_SHIELDS, default=[]): cv.ensure_list(cv.string_strict),
        # Only needed when a shield doesn't live under board_source:'s (or the SDK's
        # own) root -- see _resolve_shield_source().
        cv.Optional(CONF_SHIELD_SOURCE): _SHIELD_SOURCE_SCHEMA,
        # Only needed when a snippet doesn't live under board_source:'s (or the
        # SDK's own) root -- see _resolve_snippet_source().
        cv.Optional(CONF_SNIPPET_SOURCE): _SNIPPET_SOURCE_SCHEMA,
        # Which SDK to build against (type:), its version, where to fetch it from
        # (source:), and how often to recheck a git source: (refresh:) -- see
        # resolve_framework_version() in variants/__init__.py for the full resolution
        # logic.
        cv.Optional(CONF_FRAMEWORK, default={}): _FRAMEWORK_SCHEMA,
        # Overrides requirements_west.txt's pinned west/ninja version, decoupled from
        # CONF_VERSION since their release cadence isn't tied to Zephyr's.
        cv.Optional(CONF_WEST_VERSION): cv.string_strict,
        cv.Optional(CONF_NINJA_VERSION): cv.string_strict,
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
        # A name already prefixed SB_CONFIG_ instead targets zephyr/sysbuild.conf itself
        # (the sysbuild superproject's own Kconfig tree, e.g. a vendor SDK's `choice`
        # default in its own Kconfig.sysbuild) -- see zephyr_set_sysbuild_conf_override().
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
        # No secondary slot -- serial/wired flashing only, no live OTA path.
        cv.Optional(CONF_SINGLE_SLOT, default=False): cv.boolean,
        # Raw devicetree overlay passthrough -- lets a stock upstream board's DT be
        # patched (e.g. a partition table redefined for MCUboot) without forking a
        # custom board. "mcuboot" targets the sysbuild bootloader child image.
        cv.Optional(CONF_OVERLAYS, default={}): cv.Schema(
            {
                cv.Optional("app"): cv.string,
                cv.Optional("mcuboot"): cv.string,
            }
        ),
        # Additive west modules -- either a user's own out-of-tree module (source:),
        # or a version-only override of a module a component already requested (e.g.
        # zigbee:'s ncs-zigbee) -- see resolve_zephyr_modules().
        cv.Optional(CONF_MODULES, default=[]): cv.ensure_list(_MODULE_SCHEMA),
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

    # board.yml usually lives under boards/<vendor>/<bare name>/, even if `board:` is
    # fully qualified and/or carries an "@<revision>" suffix -- but "bare name" isn't
    # universal: most vendors (Renesas, Nordic, Raspberry Pi, ...) name the directory
    # identically to board.yml's own `name:`, but some (e.g. Arduino's `uno_r4`, whose
    # board.yml declares `name: arduino_uno_r4`) drop the vendor prefix from the
    # directory instead. Try the fast, common-case glob first; fall back to scanning
    # every board.yml under boards/ for a matching `name:` field, same fallback
    # dts_lookup.py's own _find_board_dir() already relies on for this reason.
    parts = parse_board_string(board)
    board_yml_matches = list(root.glob(f"boards/*/{parts.name}/board.yml"))
    if not board_yml_matches:
        for board_yml in root.glob("boards/*/**/board.yml"):
            try:
                doc = yaml.safe_load(board_yml.read_text())
            except (OSError, yaml.YAMLError):
                continue
            if isinstance(doc, dict) and doc.get("board", {}).get("name") == parts.name:
                board_yml_matches = [board_yml]
                break
    if not board_yml_matches:
        raise cv.Invalid(
            f"Could not find boards/*/{parts.name}/board.yml under this board_source. "
            f"Please check the source contains that board.",
            [CONF_BOARD_SOURCE],
        )
    if parts.revision is not None:
        board_dir = board_yml_matches[0].parent
        resolved, declares_revisions = resolve_revision(board_dir, parts.revision)
        if not declares_revisions:
            raise cv.Invalid(
                f"Board {parts.name!r} does not declare any revisions, but "
                f"'{CONF_BOARD}' requested '@{parts.revision}'.",
                [CONF_BOARD],
            )
        if resolved is None:
            revisions = declared_revisions(board_dir)
            raise cv.Invalid(
                f"Revision {parts.revision!r} is not valid for board {parts.name!r}. "
                f"Declared revisions: {', '.join(revisions)}",
                [CONF_BOARD],
            )
    return root


def _resolve_extra_source(
    config: ConfigType,
    items: list[str],
    board_root: Path | None,
    *,
    source_key: str,
    items_key: str,
    marker_relpath: Callable[[str], str],
    kind_label: str,
) -> Path | None:
    """Resolve a zephyr: <kind>_source: into a local root directory.

    Shared by _resolve_shield_source()/_resolve_snippet_source() -- both mirror
    board_source:'s own git/local resolution exactly, differing only in the config
    key and the per-item existence-check path. <kind>_source: is optional even when
    <items>: is set -- the root defaults to board_root (a shield/snippet
    accompanying a custom board conventionally lives alongside that root's boards/,
    mirroring Zephyr's own repo layout and its own SHIELD_ROOT/SNIPPET_ROOT
    default), or to None (let west/dts_lookup search the SDK's own tree) when
    board_source: wasn't set either.

    Existence is validated against whichever root is ultimately used -- including
    the board_root fallback, not just an explicit <kind>_source: override -- so a
    typo'd or genuinely missing item is caught here at config time instead of
    silently falling through to a debug-only "not found" warning during DTS
    validation.
    """
    explicit = source_key in config
    if not explicit:
        root = board_root
    else:
        conf = config[source_key]
        if conf[CONF_TYPE] == TYPE_GIT:
            with cv.prepend_path([source_key]):
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

    if root is None:
        # No override and no board_source: either -- nothing to validate against,
        # same as a bare `board:` with no board_source: (checked later, at DTS
        # validation time, against the SDK's own tree instead).
        return None

    error_path = [source_key] if explicit else [items_key]
    source_desc = (
        f"this {source_key}"
        if explicit
        else f"'{CONF_BOARD_SOURCE}' (the default root, since '{source_key}' wasn't set)"
    )
    for item in items:
        if not (root / marker_relpath(item)).is_file():
            raise cv.Invalid(
                f"Could not find {marker_relpath(item)} under {source_desc}. "
                f"Please check the source contains that {kind_label}, or set "
                f"'{source_key}' explicitly.",
                error_path,
            )
    return root


def _resolve_shield_source(
    config: ConfigType, shields: list[str], board_root: Path | None
) -> Path | None:
    """Resolve zephyr: shield_source: into a local SHIELD_ROOT directory."""
    return _resolve_extra_source(
        config,
        shields,
        board_root,
        source_key=CONF_SHIELD_SOURCE,
        items_key=CONF_SHIELDS,
        marker_relpath=lambda shield: f"boards/shields/{shield}/shield.yml",
        kind_label="shield",
    )


def _resolve_snippet_source(
    config: ConfigType, snippets: list[str], board_root: Path | None
) -> Path | None:
    """Resolve zephyr: snippet_source: into a local SNIPPET_ROOT directory."""
    return _resolve_extra_source(
        config,
        snippets,
        board_root,
        source_key=CONF_SNIPPET_SOURCE,
        items_key=CONF_SNIPPETS,
        marker_relpath=lambda snippet: f"snippets/{snippet}/snippet.yml",
        kind_label="snippet",
    )


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
    if variant == ZEPHYR_VARIANT_NATIVE_SIM:
        if CONF_WATCHDOG_TIMEOUT in config:
            raise cv.Invalid(
                f"'{CONF_WATCHDOG_TIMEOUT}' is not valid for variant "
                f"'{ZEPHYR_VARIANT_NATIVE_SIM}' (no real watchdog hardware)",
                [CONF_WATCHDOG_TIMEOUT],
            )
    else:
        max_timeout_ms = VARIANTS[variant].watchdog_max_timeout_ms
        # A variant whose ceiling sits below the normal 5s floor (e.g. RA4M1's
        # 5000ms) can't reach 5s either -- push the floor down to match.
        min_timeout_ms = 5000
        if max_timeout_ms is not None and max_timeout_ms < min_timeout_ms:
            min_timeout_ms = max_timeout_ms
        explicit_timeout = config.get(CONF_WATCHDOG_TIMEOUT)
        if explicit_timeout is not None and explicit_timeout.total_milliseconds != 0:
            explicit_ms = explicit_timeout.total_milliseconds
            if explicit_ms < min_timeout_ms or (
                max_timeout_ms is not None and explicit_ms > max_timeout_ms
            ):
                raise cv.Invalid(
                    f"'{CONF_WATCHDOG_TIMEOUT}' of {explicit_timeout} is outside what "
                    f"variant '{variant}' can actually arm ({min_timeout_ms}-"
                    f"{max_timeout_ms if max_timeout_ms is not None else 60000} ms) -- "
                    "use 0 to disable the watchdog instead, or a value in that range",
                    [CONF_WATCHDOG_TIMEOUT],
                )
        # 10s: covers ZBOSS's heavy CPU usage during zigbee startup without needing a
        # separate zigbee-conditional bump (platform: nrf52's own equivalent) -- the
        # plain default was arbitrary anyway. Clamped down to the variant's own
        # achievable ceiling when that's lower (see watchdog_max_timeout_ms).
        default_timeout = cv.TimePeriod(seconds=10)
        if (
            max_timeout_ms is not None
            and max_timeout_ms < default_timeout.total_milliseconds
        ):
            default_timeout = cv.TimePeriod(milliseconds=max_timeout_ms)
        config.setdefault(CONF_WATCHDOG_TIMEOUT, default_timeout)
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
    shields = config.get(CONF_SHIELDS, [])
    if CONF_SHIELD_SOURCE in config and not shields:
        raise cv.Invalid(
            f"'{CONF_SHIELD_SOURCE}' requires '{CONF_SHIELDS}' to also be set",
            [CONF_SHIELD_SOURCE],
        )
    shield_root = (
        _resolve_shield_source(config, shields, board_root) if shields else None
    )
    snippets = config.get(CONF_SNIPPETS, [])
    if CONF_SNIPPET_SOURCE in config and not snippets:
        raise cv.Invalid(
            f"'{CONF_SNIPPET_SOURCE}' requires '{CONF_SNIPPETS}' to also be set",
            [CONF_SNIPPET_SOURCE],
        )
    snippet_root = (
        _resolve_snippet_source(config, snippets, board_root) if snippets else None
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
    elif variant == ZEPHYR_VARIANT_ESP32_C5:
        from .variants.esp32_c5 import config_schema as _esp32_c5_config_schema

        config = _esp32_c5_config_schema(config)
    elif variant == ZEPHYR_VARIANT_ESP32_C3:
        from .variants.esp32_c3 import config_schema as _esp32_c3_config_schema

        config = _esp32_c3_config_schema(config)
    elif variant == ZEPHYR_VARIANT_NATIVE_SIM:
        from .variants.native_sim import config_schema as _native_sim_config_schema

        config = _native_sim_config_schema(config)
    elif variant == ZEPHYR_VARIANT_NRF52:
        from .variants.nrf52 import config_schema as _nrf52_config_schema

        config = _nrf52_config_schema(config)
    elif variant == ZEPHYR_VARIANT_NRF54L15:
        from .variants.nrf54l15 import config_schema as _nrf54l15_config_schema

        config = _nrf54l15_config_schema(config)
    elif variant == ZEPHYR_VARIANT_NRF54LM20A:
        from .variants.nrf54lm20a import config_schema as _nrf54lm20a_config_schema

        config = _nrf54lm20a_config_schema(config)
    elif variant == ZEPHYR_VARIANT_EFR32MG24:
        from .variants.efr32mg24 import config_schema as _efr32mg24_config_schema

        config = _efr32mg24_config_schema(config)
    elif variant == ZEPHYR_VARIANT_STM32L4:
        from .variants.stm32l4 import config_schema as _stm32_config_schema

        config = _stm32_config_schema(config)
    elif variant == ZEPHYR_VARIANT_STM32F4:
        from .variants.stm32f4 import config_schema as _stm32f4_config_schema

        config = _stm32f4_config_schema(config)
    elif variant == ZEPHYR_VARIANT_STM32WB55:
        from .variants.stm32wb55 import config_schema as _stm32wb55_config_schema

        config = _stm32wb55_config_schema(config)
    elif variant == ZEPHYR_VARIANT_STM32F1:
        from .variants.stm32f1 import config_schema as _stm32f1_config_schema

        config = _stm32f1_config_schema(config)
    elif variant == ZEPHYR_VARIANT_STM32U5:
        from .variants.stm32u5 import config_schema as _stm32u5_config_schema

        config = _stm32u5_config_schema(config)
    elif variant == ZEPHYR_VARIANT_RA4M1:
        from .variants.ra4m1 import config_schema as _ra4m1_config_schema

        config = _ra4m1_config_schema(config)
    elif variant == ZEPHYR_VARIANT_RP2040:
        from .variants.rp2040 import config_schema as _rp2040_config_schema

        config = _rp2040_config_schema(config)
    elif variant == ZEPHYR_VARIANT_RP2350:
        from .variants.rp2350 import config_schema as _rp2350_config_schema

        config = _rp2350_config_schema(config)
    else:
        raise cv.Invalid(f"Variant {variant!r} has no config schema registered yet")
    if config[CONF_SINGLE_SLOT] and zephyr_data()[KEY_BOOTLOADER] != BOOTLOADER_MCUBOOT:
        raise cv.Invalid(
            f"'{CONF_SINGLE_SLOT}: true' requires the MCUboot bootloader "
            f"(set 'advanced: bootloader: {BOOTLOADER_MCUBOOT}' for this variant)",
            [CONF_SINGLE_SLOT],
        )
    zephyr_data()[KEY_BOARD_ROOT] = board_root
    zephyr_data()[KEY_SHIELDS] = shields
    zephyr_data()[KEY_SHIELD_ROOT] = shield_root
    zephyr_data()[KEY_SNIPPET_ROOT] = snippet_root
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
        family=zephyr_variant_family(),
    )

    from .dts_lookup import (
        log_board_capabilities,
        validate_board,
        validate_board_revision,
    )

    board = zephyr_data()["board"]
    board_valid = validate_board(board)
    if board_valid is False:
        raise EsphomeError(
            f"Board '{board}' was not found. Check the board name, or "
            f"'{CONF_BOARD_SOURCE}' if using a custom board."
        )
    if board_valid is None:
        _LOGGER.warning(
            "[zephyr] Could not verify board '%s' exists -- board DTS files were "
            "unavailable (see earlier warning). If '%s' is misspelled, this will "
            "fail later inside the build instead of here.",
            board,
            board,
        )
    elif validate_board_revision(board) is False:
        raise EsphomeError(
            f"Board '{board}' requests a revision that is not valid for that board. "
            f"Check the '@<revision>' suffix in '{CONF_BOARD}'."
        )
    log_board_capabilities(
        board,
        variant,
        VARIANTS[variant],
        str(CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]),
        zephyr_data()["board_root"],
        zephyr_data()[KEY_SHIELDS],
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
    if variant == ZEPHYR_VARIANT_ESP32_C5:
        from .variants.esp32_c5 import to_code as _esp32_c5_to_code

        await _esp32_c5_to_code(config)
        return
    if variant == ZEPHYR_VARIANT_ESP32_C3:
        from .variants.esp32_c3 import to_code as _esp32_c3_to_code

        await _esp32_c3_to_code(config)
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
    if variant == ZEPHYR_VARIANT_NRF54LM20A:
        from .variants.nrf54lm20a import to_code as _nrf54lm20a_to_code

        await _nrf54lm20a_to_code(config)
        return
    if variant == ZEPHYR_VARIANT_EFR32MG24:
        from .variants.efr32mg24 import to_code as _efr32mg24_to_code

        await _efr32mg24_to_code(config)
        return
    if variant == ZEPHYR_VARIANT_STM32L4:
        from .variants.stm32l4 import to_code as _stm32_to_code

        await _stm32_to_code(config)
        return
    if variant == ZEPHYR_VARIANT_STM32F4:
        from .variants.stm32f4 import to_code as _stm32f4_to_code

        await _stm32f4_to_code(config)
        return
    if variant == ZEPHYR_VARIANT_STM32WB55:
        from .variants.stm32wb55 import to_code as _stm32wb55_to_code

        await _stm32wb55_to_code(config)
        return
    if variant == ZEPHYR_VARIANT_STM32F1:
        from .variants.stm32f1 import to_code as _stm32f1_to_code

        await _stm32f1_to_code(config)
        return
    if variant == ZEPHYR_VARIANT_STM32U5:
        from .variants.stm32u5 import to_code as _stm32u5_to_code

        await _stm32u5_to_code(config)
        return
    if variant == ZEPHYR_VARIANT_RA4M1:
        from .variants.ra4m1 import to_code as _ra4m1_to_code

        await _ra4m1_to_code(config)
        return
    if variant == ZEPHYR_VARIANT_RP2040:
        from .variants.rp2040 import to_code as _rp2040_to_code

        await _rp2040_to_code(config)
        return
    if variant == ZEPHYR_VARIANT_RP2350:
        from .variants.rp2350 import to_code as _rp2350_to_code

        await _rp2350_to_code(config)
        return
    raise NotImplementedError(f"Zephyr variant {variant!r} has no to_code registered")


def _signed_image_flash_address(signed_hex: Path) -> int | None:
    """Return the absolute flash address imgtool wrote the signed image at, by
    reading the Intel HEX records at the top of the file -- signed.bin (raw binary)
    carries no address of its own, and this is the build's own authoritative value
    rather than one derived/guessed locally.
    """
    with Path(signed_hex).open(encoding="ascii") as f:
        extended_line = f.readline().strip()
        data_line = f.readline().strip()
    # ":02" byte count, "0000" record address, "04" = extended linear address,
    # followed by the upper 16 bits of the 32-bit address.
    if not extended_line.startswith(":02000004"):
        return None
    upper16 = int(extended_line[9:13], 16)
    # Data record: ":<len><addr16><00><data...><checksum>" -- addr16 is the low 16
    # bits of this record's address, not necessarily 0 (e.g. non-64KB-aligned slots).
    if len(data_line) < 9 or data_line[7:9] != "00":
        return None
    lower16 = int(data_line[3:7], 16)
    return (upper16 << 16) | lower16


def _find_picotool() -> Path | None:
    import shutil
    import sys

    from esphome.util import PICOTOOL_PACKAGE

    binary_name = "picotool.exe" if sys.platform == "win32" else "picotool"
    pio_packages = Path.home() / ".platformio" / "packages"
    picotool = pio_packages / PICOTOOL_PACKAGE / binary_name
    if not picotool.is_file():
        picotool = Path(shutil.which(binary_name) or "")
    return picotool if picotool and picotool.is_file() else None


def _touch_1200_baud_reboot(port: str, timeout: float = 10.0) -> bool:
    """Trigger an RP2040/RP2350 running ESPHome's own firmware (with
    USE_ZEPHYR_BOOTSEL_TOUCH's poll loop active) to reboot into BOOTSEL, by briefly
    opening its USB CDC serial port at 1200 baud -- the cross-ecosystem "magic baud
    rate" convention -- then waiting for it to re-enumerate in BOOTSEL mode.
    """
    import time

    import serial

    from esphome.util import get_serial_ports

    picotool = _find_picotool()
    if picotool is None:
        return False

    # Once triggered, BOOTSEL mode is anonymous -- picotool can't tell devices apart
    # (no serial number/bus-address selection is used anywhere in this upload path), so
    # there's no way to prove after the fact that whatever appears in BOOTSEL is really
    # the board we touched, versus some other RP-family device on the system. The best
    # available guard is refusing up front whenever more than one serial-capable
    # candidate is present -- a device already sitting in raw BOOTSEL can't be counted
    # here too, since detecting it requires `picotool info -d`, which is not reliable
    # enough to depend on for a safety check (see _upload_using_picotool()).
    if len(get_serial_ports()) > 1:
        _LOGGER.error(
            "More than one RP2040/RP2350-capable device is connected. Disconnect all "
            "but the target device before uploading, or put the target into BOOTSEL "
            "mode manually and select it explicitly."
        )
        return False

    try:
        with serial.Serial(port, baudrate=1200):
            pass
    except serial.SerialException as err:
        _LOGGER.warning("Could not open %s at 1200 baud: %s", port, err)
        return False

    # `picotool info -d` isn't reliable enough to poll for BOOTSEL re-enumeration (see
    # _upload_using_picotool()) -- instead, wait for the touched port itself to
    # disappear, which is what actually happens when the device reboots out of CDC-ACM.
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if port not in (p.path for p in get_serial_ports()):
            return True
        time.sleep(0.5)
    return False


def _upload_using_picotool() -> bool:
    """Upload Zephyr firmware to an RP2040/RP2350 device in BOOTSEL mode using picotool."""
    import sys

    from esphome.util import is_picotool_usb_permission_error

    picotool = _find_picotool()
    if picotool is None:
        _LOGGER.error(
            "picotool not found. Install it via PlatformIO (rp2040 platform) "
            "or your system package manager."
        )
        return False

    # sysbuild produces two separate images when mcuboot is enabled: the bootloader
    # itself (linked to run from the start of flash) and the app, signed and linked
    # to run from slot0_partition instead. Unlike a direct-boot image, the app alone
    # has no valid image at the boot ROM's fixed entry address (flash offset 0), so
    # both must be written -- Zephyr's own "uf2" runner doesn't handle this correctly
    # either (its zephyr.uf2 output is always built from the unsigned binary).
    if zephyr_data()[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT:
        mcuboot_elf = CORE.relative_build_path(".west_build/mcuboot/zephyr/zephyr.elf")
        signed_bin = CORE.relative_build_path(
            ".west_build/zephyr/zephyr/zephyr.signed.bin"
        )
        signed_hex = CORE.relative_build_path(
            ".west_build/zephyr/zephyr/zephyr.signed.hex"
        )
        if (
            not mcuboot_elf.is_file()
            or not signed_bin.is_file()
            or not signed_hex.is_file()
        ):
            _LOGGER.error("MCUboot firmware not found. Compile first.")
            return False
        address = _signed_image_flash_address(signed_hex)
        if address is None:
            _LOGGER.error(
                "Could not determine the signed app image's flash address from %s.",
                signed_hex,
            )
            return False
        # (file, offset, execute-after-load) -- mcuboot first (no execute, so the
        # device stays in BOOTSEL for the second write), then the signed app.
        loads: list[tuple[Path, int | None, bool]] = [
            (mcuboot_elf, None, False),
            (signed_bin, address, True),
        ]
    else:
        elf = CORE.relative_build_path(".west_build/zephyr/zephyr/zephyr.elf")
        if not elf.is_file():
            _LOGGER.error("Zephyr firmware ELF not found. Compile first.")
            return False
        loads = [(elf, None, True)]

    for file_path, offset, execute in loads:
        _LOGGER.info("Uploading %s via picotool...", file_path.name)
        cmd = [str(picotool), "load"]
        if offset is not None:
            cmd += ["-o", hex(offset)]
        if execute:
            cmd.append("-x")
        cmd.append(str(file_path))
        try:
            result = subprocess.run(
                cmd,
                stderr=subprocess.PIPE,
                timeout=60,
                check=False,
            )
        except subprocess.TimeoutExpired:
            _LOGGER.error("picotool upload timed out after 60 seconds.")
            return False
        except OSError as err:
            _LOGGER.error("Failed to run picotool: %s", err)
            return False

        if result.returncode != 0:
            stderr = result.stderr.decode("utf-8", errors="replace").strip()
            if stderr:
                for line in stderr.splitlines():
                    _LOGGER.error("picotool: %s", line)
            if is_picotool_usb_permission_error(stderr):
                msg = "Permission denied accessing USB device."
                if sys.platform.startswith("linux"):
                    from esphome.__main__ import _RP2040_UDEV_HINT

                    msg += f" {_RP2040_UDEV_HINT}"
                _LOGGER.error(msg)
            else:
                _LOGGER.error(
                    "picotool upload failed (exit code %d).", result.returncode
                )
            return False
    return True


def upload_program(config: ConfigType, args, host: str) -> bool:
    if KEY_ZEPHYR not in CORE.data:
        zephyr_config = config.get(CORE.target_platform)
        if not zephyr_config:
            raise EsphomeError(
                "Zephyr platform configuration is missing; "
                "please re-validate and recompile."
            )
        CONFIG_SCHEMA(zephyr_config)

    if host == "BOOTSEL":
        if zephyr_data().get(KEY_RUNNER):
            _LOGGER.info(
                "Configured advanced.runner has no effect on BOOTSEL/picotool flashing"
            )
        return _upload_using_picotool()

    if host == "PYOCD":
        if zephyr_variant_family() == "esp32":
            return False  # PYOCD isn't supported on esp32-family; it flashes over serial instead

        configured_runner = zephyr_data().get(KEY_RUNNER)
        if configured_runner and configured_runner != "pyocd":
            _LOGGER.info(
                "Ignoring configured advanced.runner '%s' -- --device PYOCD always "
                "forces the pyocd runner",
                configured_runner,
            )

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
            modules=resolve_zephyr_modules(),
        )
        build_dir = CORE.relative_build_path(".west_build")
        if not run_west_flash_pyocd(python_bin, framework_path, west_env, build_dir):
            raise EsphomeError("Zephyr pyocd flash failed")
        return True

    # rpi_pico-family variants (RP2040/RP2350) given a normal serial port instead of
    # BOOTSEL: the board's default west runner (uf2) only works once already in
    # BOOTSEL, so trigger that ourselves first via the 1200-baud touch (see
    # USE_ZEPHYR_BOOTSEL_TOUCH), then flash the same way `--device BOOTSEL` would.
    if zephyr_variant_family() == "rpi_pico":
        from esphome.upload_targets import PortType, get_port_type

        if get_port_type(host) != PortType.SERIAL:
            return False

        if zephyr_data().get(KEY_RUNNER):
            _LOGGER.info(
                "Configured advanced.runner has no effect on BOOTSEL/picotool flashing"
            )
        if not _touch_1200_baud_reboot(host):
            raise EsphomeError(
                f"Could not trigger a BOOTSEL reboot on {host}. Manually put the "
                "device into BOOTSEL mode (hold BOOTSEL while plugging in) and retry."
            )
        return _upload_using_picotool()

    # Every esp32-family variant flashes the same way: Zephyr's generic esp32
    # runner (runners/esp32.py, wrapping esptool) via `west flash`.
    if zephyr_variant_family() == "esp32":
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
            modules=resolve_zephyr_modules(),
        )

        build_dir = CORE.relative_build_path(".west_build")
        speed = getattr(args, "upload_speed", None)

        if not run_west_flash(
            python_bin,
            framework_path,
            west_env,
            build_dir,
            host,
            speed,
            runner=zephyr_data().get(KEY_RUNNER),
        ):
            raise EsphomeError("Zephyr west flash failed")
        return True

    # Non-ESP32 Zephyr variants (for example EFR32/nRF in SDK-Zephyr mode) are
    # generally flashed by the board's default west runner (jlink, pyocd, etc.)
    # rather than esptool over a selected serial port.
    from esphome.upload_targets import PortType, get_port_type

    if get_port_type(host) != PortType.SERIAL:
        return False

    from .build_zephyr import resolve_dev_id, run_west_flash_generic
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
        modules=resolve_zephyr_modules(),
    )
    build_dir = CORE.relative_build_path(".west_build")
    configured_runner = zephyr_data().get(KEY_RUNNER)
    # Disambiguate which attached probe to flash when the effective runner supports
    # device IDs -- see resolve_dev_id() for why this can't just always be forwarded,
    # and #85 for the multi-probe problem it fixes.
    dev_id = resolve_dev_id(
        python_bin,
        framework_path,
        build_dir,
        host,
        runner_override=configured_runner,
    )
    if dev_id:
        _LOGGER.info("Selecting probe %s (port %s) for west flash", dev_id, host)
    if not run_west_flash_generic(
        python_bin,
        framework_path,
        west_env,
        build_dir,
        dev_id,
        runner=configured_runner,
    ):
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
    sdk_name, sdk = resolve_sdk(variant_data, zephyr_data().get(KEY_FRAMEWORK_TYPE))
    version = str(CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION])
    modules = resolve_zephyr_modules()
    # A local_path module isn't a west project (west_install() only handles the
    # git-sourced ones) -- it's an already-on-disk directory, wired in the same way a
    # converted PlatformIO library is: EXTRA_ZEPHYR_MODULES, no west/manifest
    # involvement at all.
    extra_modules.extend(m.local_path for m in modules if m.local_path is not None)
    python_bin, framework_path, west_env = west_install(
        sdk,
        version,
        zephyr_data()["west_version"],
        zephyr_data()["ninja_version"],
        zephyr_data()["sdk_source"],
        config[CORE.target_platform][CONF_FRAMEWORK][CONF_REFRESH],
        modules=modules,
    )
    if sdk_name == "silabs":
        from .commander_setup import check_and_install as commander_install

        # "commander_version" mirrors efr32mg24.CONF_COMMANDER_VERSION -- not imported
        # directly to avoid this generic module depending on one specific variant.
        commander_version = (
            config[CORE.target_platform].get(CONF_ADVANCED, {}).get("commander_version")
        )
        commander_dir = commander_install(commander_version)
        west_env["PATH"] = f"{commander_dir}{os.pathsep}{west_env['PATH']}"
    cross_toolchain = variant_data.toolchain
    sdk_dir = sdk_install(framework_path, toolchain=cross_toolchain)
    blob_specs = list(zephyr_data()["blobs"])
    if variant_data.blobs:
        blob_specs.append(variant_data.blobs)
    for module, allow_regex, sentinel_name in blob_specs:
        run_west_blobs_fetch(
            python_bin, framework_path, west_env, module, allow_regex, sentinel_name
        )
    for zmodule in modules:
        if zmodule.blobs is None:
            continue
        blob_module, allow_regex, sentinel_name = zmodule.blobs
        run_west_blobs_fetch(
            python_bin,
            framework_path,
            west_env,
            blob_module,
            allow_regex,
            sentinel_name,
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
        shield_root=zephyr_data().get(KEY_SHIELD_ROOT),
        shields=zephyr_data()[KEY_SHIELDS],
        snippet_root=zephyr_data().get(KEY_SNIPPET_ROOT),
        requested_runner=zephyr_data().get(KEY_RUNNER),
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


# The PC bound matches the gate in platform_hooks.STACKTRACE_GATES;
# the logger prints both registers with %08x, so a real PC is always
# 8 digits. tests/unit_tests/test_stacktrace.py guards against drift.
STACKTRACE_ZEPHYR_PC_LR_RE = re.compile(r"PC=(0x[0-9a-fA-F]{3,})\s+LR=(0x[0-9a-fA-F]+)")


def process_stacktrace(config: ConfigType, line: str, backtrace_state: bool) -> bool:
    if "Last crash:" in line:
        return True
    if backtrace_state:
        match = STACKTRACE_ZEPHYR_PC_LR_RE.search(line)
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
