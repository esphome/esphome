from collections.abc import Callable
from dataclasses import dataclass, field
import importlib
import logging
from pathlib import Path

import esphome.config_validation as cv
from esphome.const import (
    CONF_VERSION,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_ZEPHYR,
    Toolchain,
)
from esphome.core import CORE
from esphome.types import ConfigType

from ..const import (
    CONF_NINJA_VERSION,
    CONF_WEST_VERSION,
    KEY_ZEPHYR,
    VERSION_RECOMMENDED,
)

_LOGGER = logging.getLogger(__name__)


@dataclass
class ZephyrSDK:
    manifest_url: str
    manifest_rev: str = "recommended"
    toolchain_url: str | None = (
        None  # None = host compiler or PlatformIO-managed toolchain
    )
    boards_repo_url: str | None = (
        None  # repo containing boards/; defaults to manifest_url when None
    )
    tools_subdir: str | None = (
        None  # dir name under CORE.data_dir where native SDK is installed (e.g. "sdk-nrf")
    )
    # Default/minimum supported Zephyr version; a variant overrides these via its own
    # *_version_override fields. 4.4.1: fixes a C++ compile error in ethernet.h that any
    # wifi_mgmt.h consumer hits.
    default_version: str = "4.4.1"
    min_version: cv.Version = cv.Version(4, 4, 1)


@dataclass
class ZephyrVariant:
    sdk: ZephyrSDK
    # Groups sibling variants sharing the same silicon vendor (e.g. "esp32" for
    # esp32_h2/esp32_c6), so shared config/codegen can gate on family once instead
    # of an ever-growing per-variant elif chain. None = no family.
    family: str | None = None
    boards: list[str] = field(default_factory=list)  # empty = accept any board
    # Per-variant DTS pin extractors keyed by bus name (e.g. "i2c", "spi").
    # Signature: (board: str, bus_label: str) -> {"sda": int, "scl": int} | None
    # Absent key = variant does not support DTS pin extraction for that bus.
    pinctrl_extractors: dict[str, Callable[[str, str], dict[str, int] | None]] = field(
        default_factory=dict
    )
    # Toolchain identifiers supported by this variant (StrEnum values from esphome.const.Toolchain).
    valid_toolchains: tuple[str, ...] = ("platformio",)
    # Binary blobs required by this variant: (west_module, allow_regex, sentinel_name).
    # allow_regex filters the module's blob list to the chip-specific subset, though
    # some HALs (e.g. hal_espressif with BLE) need ".*" since Zephyr's CMake verifies
    # the entire manifest regardless of chip. sentinel_name gates the one-time fetch.
    # None means no blobs are needed.
    blobs: tuple[str, str, str] | None = None
    # Pins per GPIO devicetree port node, used to split a flat pin number into
    # (port, pin-within-port). Varies by vendor/SoC family (32 for nRF52/Espressif,
    # 16 for STM32), so this lives per variant rather than a hardcoded constant.
    gpio_port_width: int = 32
    # ESPHome components this variant can actually support. Named after the
    # component/protocol, not the raw radio -- e.g. `openthread` and `zigbee` both need
    # 802.15.4, but esp32-family only has the former ported (ZBOSS is Nordic-only).
    # Known values: "wifi", "ble", "openthread", "zigbee", "ethernet", "modem".
    transports: frozenset[str] = frozenset()
    # Per-transport Zephyr driver facts, keyed by transport name. (kconfig, dt_label):
    # kconfig is the driver's top-level Kconfig symbol (e.g. "WIFI_ESP32"), dt_label the
    # devicetree node to mark `status = "okay"`. Only transports needing this simple
    # "enable one Kconfig + one DT node" shape use it -- openthread/zigbee configure
    # their own Kconfig directly instead.
    transport_drivers: dict[str, tuple[str, str]] = field(default_factory=dict)
    # Per-variant override of the SDK's default version window (see ZephyrSDK); None uses
    # the SDK's value unchanged.
    default_version_override: str | None = None
    min_version_override: cv.Version | None = None
    # West board target segments (e.g. "esp32c6"/"hpcore"), used by qualify_board() to
    # expand a bare `board:` name. None = no segment (native_sim uses its own literal
    # board string instead).
    soc: str | None = None
    qualifier: str | None = None
    # MCUboot upgrade modes this variant's upstream port actually supports -- checked
    # against ota: platform: esphome's swap_method:. Empty = no real MCUboot/OTA rollback
    # applies (native_sim).
    swap_methods: frozenset[str] = frozenset()
    # GPIO -> ADC1 channel index, from the vendor SoC header (not discoverable from any
    # board's DTS -- ADC pins are fixed-function silicon). Empty = no ADC1 support wired up.
    adc1_channel_map: dict[int, int] = field(default_factory=dict)
    # GPIOs the ESP32 GPIO matrix can route to a UART TX/RX signal, from Zephyr's
    # dt-bindings pinctrl header -- a fixed per-chip pinmux fact, not always identical
    # between tx/rx (e.g. original ESP32's GPIO34-39 are RX-only). Empty = no
    # GPIO-matrix UART pin override wired up.
    uart_valid_pins: dict[str, frozenset[int]] = field(default_factory=dict)

    @property
    def default_version(self) -> str:
        return self.default_version_override or self.sdk.default_version

    @property
    def min_version(self) -> cv.Version:
        return self.min_version_override or self.sdk.min_version


def qualify_board(variant: ZephyrVariant, board: str) -> str:
    """Expand a bare board name into its full west target string. Already-qualified input
    is left untouched."""
    if "/" in board or variant.soc is None:
        return board
    qualified = f"{board}/{variant.soc}"
    if variant.qualifier:
        qualified += f"/{variant.qualifier}"
    return qualified


def warn_if_not_recommended_version(
    variant: ZephyrVariant, framework_ver: cv.Version
) -> None:
    if framework_ver != cv.Version.parse(variant.default_version):
        _LOGGER.warning(
            "The selected Zephyr version is not the recommended one (%s). "
            "If there are connectivity or build issues please remove the manual version.",
            variant.default_version,
        )


def resolve_framework_version(
    variant: ZephyrVariant, variant_name: str, config: ConfigType, requires_reason: str
) -> tuple[str, cv.Version]:
    """Resolve and validate the configured Zephyr version for a variant's config_schema().

    Returns (version_str, parsed_version) for the caller to store. Raises cv.Invalid if
    below variant.min_version (requires_reason explains why, e.g. "mainline ESP32-C6
    support"); only warns (doesn't block) if it differs from variant.default_version.
    """
    version_str = str(
        config.get(
            CONF_VERSION, config.get(KEY_FRAMEWORK_VERSION, variant.default_version)
        )
    )
    if version_str == VERSION_RECOMMENDED:
        version_str = variant.default_version
    framework_ver = cv.Version.parse(version_str)
    if framework_ver < variant.min_version:
        raise cv.Invalid(
            f"{variant_name} requires Zephyr >= {variant.min_version} ({requires_reason}). "
            f"Got {version_str}.",
            [CONF_VERSION],
        )
    warn_if_not_recommended_version(variant, framework_ver)
    return version_str, framework_ver


def set_core_data(
    variant_name: str,
    board: str,
    bootloader: str,
    framework_ver: cv.Version,
    config: ConfigType,
    *,
    prj_conf: dict | None = None,
    sysbuild_conf: dict | None = None,
    overlay: dict | None = None,
    extra_build_files: dict | None = None,
    pm_static: list | None = None,
    user: dict | None = None,
    kconfig: str = "",
    fake_board_manifest: str | None = None,
    dts_base_path: str | None = None,
    i2c_bus_cache: dict | None = None,
    cpp_path: str = "",
    board_dir_cache: dict | None = None,
    dts_include_paths: list | None = None,
    board_edt_cache: dict | None = None,
    board_yaml_cache: dict | None = None,
    sysbuild: bool = False,
    board_root: Path | None = None,
) -> None:
    """Populate CORE.data for a Zephyr variant's config_schema().

    `config` supplies cross-variant user overrides (CONF_WEST_VERSION, CONF_NINJA_VERSION).
    Every keyword defaults to today's shared "nothing configured yet" starting point. A
    future variant (e.g. Nordic, STM32) that genuinely needs a different starting value for
    any of these passes its own instead of forking this function.
    """
    from .. import (
        ZephyrData,  # deferred: zephyr/__init__.py imports this package at load time
    )

    CORE.toolchain = Toolchain.SDK_ZEPHYR
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_ZEPHYR
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = KEY_ZEPHYR
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = framework_ver

    CORE.data[KEY_ZEPHYR] = ZephyrData(
        board=board,
        board_root=board_root,
        sdk_source=None,
        bootloader=bootloader,
        variant=variant_name,
        prj_conf=prj_conf if prj_conf is not None else {},
        sysbuild_conf=sysbuild_conf if sysbuild_conf is not None else {},
        overlay=overlay if overlay is not None else {"": ""},
        extra_build_files=extra_build_files if extra_build_files is not None else {},
        pm_static=pm_static if pm_static is not None else [],
        user=user if user is not None else {},
        kconfig=kconfig,
        fake_board_manifest=fake_board_manifest,
        dts_base_path=dts_base_path,
        i2c_bus_cache=i2c_bus_cache if i2c_bus_cache is not None else {},
        cpp_path=cpp_path,
        board_dir_cache=board_dir_cache if board_dir_cache is not None else {},
        dts_include_paths=dts_include_paths,
        board_edt_cache=board_edt_cache if board_edt_cache is not None else {},
        board_yaml_cache=board_yaml_cache if board_yaml_cache is not None else {},
        sysbuild=sysbuild,
        west_version=config.get(CONF_WEST_VERSION),
        ninja_version=config.get(CONF_NINJA_VERSION),
    )


MAINLINE: ZephyrSDK = ZephyrSDK(
    manifest_url="https://github.com/zephyrproject-rtos/zephyr",
    tools_subdir="sdk-zephyr",
)

# Module names under this package that define a variant.
# Each module must export VARIANT_NAME and a ZephyrVariant instance named VARIANT.
_VARIANT_MODULES = [
    "esp32",
    "esp32_h2",
    "esp32_c6",
    "native_sim",
]


class _LazyVariants(dict):
    """Defers variant module loading until first access to avoid circular imports."""

    _built: bool = False

    def _ensure(self) -> None:
        if self._built:
            return
        self._built = True
        for mod_name in _VARIANT_MODULES:
            mod = importlib.import_module(f".{mod_name}", package=__name__)
            self[mod.VARIANT_NAME] = mod.VARIANT

    def __getitem__(self, key):
        self._ensure()
        return super().__getitem__(key)

    def __contains__(self, key):
        self._ensure()
        return super().__contains__(key)

    def __iter__(self):
        self._ensure()
        return super().__iter__()

    def __len__(self):
        self._ensure()
        return super().__len__()

    def items(self):
        self._ensure()
        return super().items()

    def get(self, key, default=None):
        self._ensure()
        return super().get(key, default)


VARIANTS: dict[str, ZephyrVariant] = _LazyVariants()
