from collections.abc import Callable
from dataclasses import dataclass, field
import importlib
import logging
from pathlib import Path

import esphome.config_validation as cv
from esphome.const import (
    CONF_FRAMEWORK,
    CONF_REFRESH,
    CONF_SOURCE,
    CONF_TYPE,
    CONF_URL,
    CONF_VERSION,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_ZEPHYR,
    TYPE_GIT,
    Toolchain,
)
from esphome.core import CORE, EsphomeError
from esphome.types import ConfigType

from ..const import (
    CONF_NINJA_VERSION,
    CONF_SINGLE_SLOT,
    CONF_SNIPPETS,
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
    # False: boards_repo_url is checked out directly at tag f"v{version}" (mainline's own
    # tag scheme). True: boards_repo_url's *checkout ref* isn't derivable from version at
    # all -- it must be read from manifest_url's own west.yml (at tag f"v{version}"), which
    # states the exact revision it pins for the boards_repo_url project. NCS needs this:
    # its bundled sdk-zephyr fork's tag scheme isn't a stable function of the nrf release
    # version (confirmed: NCS 2.9.2 pins zephyr at "v3.7.99-ncs2-2", NCS 3.4.0 pins it at
    # "ncs-v3.4.0" -- two different, unrelated schemes), so guessing a format string would
    # silently break on some past or future NCS release. Reading the real manifest is the
    # only correct way to know it.
    resolve_boards_ref_via_manifest: bool = False
    # Set when this SDK has no tagged release usable as a default -- framework: source: is
    # then mandatory; explains why in the raised error.
    requires_source_reason: str | None = None
    # capability -> module. A module is additive to this SDK (fetched by `west update`
    # alongside it, no boards/toolchain/version of its own) -- distinct from alt_sdks,
    # which are competing SDK roots. See ZephyrModuleTemplate.
    modules: dict[str, "ZephyrModuleTemplate"] = field(default_factory=dict)
    # West project name to use for this SDK when it's imported as a sibling project
    # rather than the manifest root (i.e. whenever modules is non-empty -- see
    # _generate_synthetic_manifest). None = derive from manifest_url's basename.
    # NCS needs this pinned to "nrf": its own bundled sysbuild/Kconfig scripts
    # hardcode that exact west project name (matching Nordic's own official
    # manifests, confirmed against ncs-zigbee's real west.yml) to generate
    # `SYSBUILD_NRF_KCONFIG`-style variables -- naming the project anything else
    # (e.g. "sdk-nrf", the URL's basename) leaves that variable unset and breaks
    # sysbuild's Kconfig configure step. Found via a real `esphome compile` (variant:
    # nrf52 + zigbee:), not just by reading Nordic's manifest -- the failure only
    # surfaces at CMake's Kconfig-configure step, not at `west update`.
    west_project_name: str | None = None


# A fully resolved, ready-to-fetch additive west module -- identical shape whether it
# came from a component's capability request or a user's own `zephyr: modules:` entry.
@dataclass
class ZephyrModule:
    name: str  # west project/module name
    manifest_url: str | None = None  # git remote; None when local_path is set
    local_path: Path | None = None  # user's `type: local` module
    revision: str = "main"  # concrete ref/tag/branch to check out
    # (west_module, allow_regex, sentinel_name) -- same shape as ZephyrVariant.blobs.
    blobs: tuple[str, str, str] | None = None


# Owned by the ZephyrSDK that can provide a capability; resolves to a concrete
# ZephyrModule given the active root SDK's version.
@dataclass
class ZephyrModuleTemplate:
    name: str
    manifest_url: str
    # root SDK's (major, minor) -> this module's matching version. Nordic pairs
    # sdk-nrf and ncs-zigbee releases but the two use unrelated numbering schemes
    # (3.4.x vs 1.4.x) -- no formula, each pairing is a real entry, added as Nordic
    # ships new paired releases.
    default_versions: dict[tuple[int, int], str] = field(default_factory=dict)
    blobs: tuple[str, str, str] | None = None
    # Lowest version this module is known to work at all -- independent of
    # default_versions' per-root-version pairing, this just rejects an obviously
    # too-old zephyr: modules: override at config time instead of failing much later
    # inside `west update`. None = not enforced (e.g. a module with no numbered
    # releases at all).
    min_version: cv.Version | None = None

    def resolve(
        self, root_version: cv.Version, explicit_version: str | None = None
    ) -> ZephyrModule:
        version = explicit_version or self.default_versions.get(
            (root_version.major, root_version.minor)
        )
        if version is None:
            raise EsphomeError(
                f"Module '{self.name}' has no known compatible version for "
                f"framework version {root_version.major}.{root_version.minor} -- "
                "specify it explicitly under zephyr: modules:."
            )
        tag = version
        try:
            parsed_version = cv.Version.parse(version)
        except ValueError:
            pass  # a git ref/branch (e.g. "main"), not a semver -- used as-is, no "v"
        else:
            if self.min_version is not None and parsed_version < self.min_version:
                raise EsphomeError(
                    f"Module '{self.name}' requires version >= {self.min_version}. "
                    f"Got {version}."
                )
            # default_versions/explicit_version are bare "X.Y.Z" (for the comparison
            # above and for a user typing zephyr: modules: version: without having to
            # know the tag scheme), but the actual git tag needs the "v" -- same
            # normalization the root SDK's own ver_tag already does.
            if not tag.startswith("v"):
                tag = f"v{tag}"
        return ZephyrModule(
            name=self.name,
            manifest_url=self.manifest_url,
            revision=tag,
            blobs=self.blobs,
        )


@dataclass
class ZephyrVariant:
    sdk: ZephyrSDK
    # `zephyr: framework: type:` key naming this variant's default `sdk` above. Every
    # variant needs one, even single-SDK ones -- it's what the framework: schema validates
    # `type:` against.
    sdk_name: str = "zephyr"
    # Additional selectable `framework: type:` values, keyed by name -- e.g. nrf52's
    # {"zephyr": MAINLINE} lets mainline Zephyr be chosen as an alternate to its NCS
    # default. Empty for single-SDK variants.
    alt_sdks: dict[str, "ZephyrSDK"] = field(default_factory=dict)
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
    # Zephyr SDK cross-compiler name (arg to setup.sh -t / ZEPHYR_TOOLCHAIN_VARIANT).
    # None = install host tools only and use the host GCC.
    toolchain: str | None = None
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
    # Devicetree node-label suffixes for this variant's GPIO ports, indexed by port
    # number -- e.g. ("a", "b", "c", "d") for Silicon Labs' gpioa/gpiob/gpioc/gpiod
    # Series 2 GPIO ports. None = numeric suffixes (gpio0, gpio1, ...), which is what
    # every DT_NODELABEL(gpio{port}) lookup has always assumed -- Nordic and Espressif
    # both use numeric node names, only their *display* prefix differs (see
    # _PORT_BANKED_FAMILIES in gpio.py). A lettered family also gets its display
    # prefix from this (e.g. "PA"), independently of Nordic's numeric "P0." style.
    gpio_port_labels: tuple[str, ...] | None = None
    # Devicetree node-label prefix for this variant's GPIO port nodes. Defaults to
    # "gpio", which happens to be right for every variant wired up so far (Nordic,
    # Espressif, Silicon Labs, STM32, RP2040) -- but that's a coincidence, not a Zephyr
    # convention: a scan of the whole Zephyr dts/ tree turns up plenty of vendors that
    # name their GPIO port nodes something else entirely (Renesas RA: ioport0/ioport1;
    # Renesas RX and RA6, Microchip SAM, NXP MCX: porta/portb; TI: main_gpio; Xilinx
    # Zynq: psgpio_bank; Infineon: gpio_prt; several RISC-V vendors: pioa/piob). Even
    # different Renesas families disagree with each other (RA4M1 vs RA6 above).
    #
    # DO NOT assume "gpio" is safe for a new variant just because it's the default here
    # and every existing variant uses it. Before adding a new ZephyrVariant, grep that
    # SoC's actual .dtsi under the Zephyr SDK for "gpio-controller;" and read the node
    # label a few lines above it -- then set this field to match. Getting it wrong is
    # silent at compile time: DEVICE_DT_GET_OR_NULL() on a nonexistent node just resolves
    # to nullptr, so every GPIO pin on the new variant fails at runtime with
    # "gpio %u is not ready." instead of a build error (see the RA4M1 bug this comment
    # is here because of).
    gpio_node_prefix: str = "gpio"
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
    # Per-transport binary blobs, keyed by transport name -- same (west_module,
    # allow_regex, sentinel_name) shape as `blobs` above, but only fetched when that
    # transport is actually configured (unlike `blobs`, which always runs for the
    # variant). Use this when the blobs are for hardware not every board of the variant
    # has (e.g. rp2040's CYW43439 wifi chip, only present on `/w`-qualified boards).
    transport_blobs: dict[str, tuple[str, str, str]] = field(default_factory=dict)
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
    # Espressif-shaped: their devicetree channel@N address IS the real silicon channel.
    adc1_channel_map: dict[int, int] = field(default_factory=dict)
    # GPIO -> SAADC analog-input name (e.g. "AIN5"), fixed-function silicon per the
    # vendor datasheet. Nordic-shaped: unlike Espressif, the devicetree channel@N
    # address is just an arbitrary config slot -- the real pin connection is this
    # AIN name, via a separate `zephyr,input-positive` property. Empty = no SAADC
    # support wired up.
    adc_ain_map: dict[int, str] = field(default_factory=dict)
    # GPIOs the ESP32 GPIO matrix can route to a UART TX/RX signal, from Zephyr's
    # dt-bindings pinctrl header -- a fixed per-chip pinmux fact, not always identical
    # between tx/rx (e.g. original ESP32's GPIO34-39 are RX-only). Empty = no
    # GPIO-matrix UART pin override wired up.
    uart_valid_pins: dict[str, frozenset[int]] = field(default_factory=dict)
    # Devicetree node labels backing the `logger: hardware_uart: UART0`/`UART1` symbolic
    # selections. ESP32-family and nRF52 boards both label their two console-capable UARTs
    # uart0/uart1, so that's the default; nRF54 series numbers peripheral instances instead
    # (e.g. uart20/uart30 on the nrf54l15dk), so that variant overrides this map.
    uart_node_labels: dict[str, str] = field(
        default_factory=lambda: {"UART0": "uart0", "UART1": "uart1"}
    )
    # Devicetree node labels of this variant's PWM peripheral instances, in
    # zephyr_pwm's block-allocation order. nRF52840 numbers them pwm0-pwm3; nRF54L
    # series numbers peripheral instances instead (pwm20-pwm22), same convention as
    # uart_node_labels above. RP2040/RP2350 expose a single "pwm" controller node
    # covering every slice, so their list repeats that same label once per slice
    # (block-allocation order still needs one entry per block). Empty = no PWM
    # support wired up.
    pwm_node_labels: list[str] = field(default_factory=list)
    # Channels per PWM block on this variant's hardware -- a block is one counter
    # with a single shared period, so this many channels must share one frequency.
    # nRF52/nRF54L's NRF_PWM peripheral has 4 channels per instance (the default).
    # RP2040/RP2350's PWM slices have only 2 channels (A/B) each. Only consulted by
    # zephyr_pwm's free (Nordic-style) block allocator -- RP2040/RP2350/RA4M1 assign
    # each pin a fixed block+channel instead (see pwm_pin_map), so this field is
    # descriptive only for those families.
    pwm_channels_per_block: int = 4
    # GPIO -> (index into pwm_node_labels, local channel within that block) for
    # variants whose PWM pins are wired to one fixed hardware position each, rather
    # than freely assignable to any channel the way Nordic's NRF_PSEL pinmux is.
    # Empty = this variant's PWM (if any) uses the free allocator instead.
    pwm_pin_map: dict[int, tuple[int, int]] = field(default_factory=dict)
    # Largest `zephyr: watchdog_timeout:` this variant's WDT hardware can actually
    # arm, in milliseconds. None = trust the generic 5-60s schema range (true for
    # every variant whose WDT clock gives it a multi-minute+ ceiling). Some
    # variants' watchdog peripheral is clocked too fast, or has too small a
    # counter, for that range -- the driver just silently substitutes a shorter
    # real timeout with no error when asked for more than it can give. Set this so
    # config validation can catch that instead of a silent boot-loop on real
    # hardware. Value should leave headroom below the real computed ceiling for
    # rounding-table granularity, not sit exactly on it.
    watchdog_max_timeout_ms: int | None = None


def resolve_sdk(
    variant: ZephyrVariant, framework_type: str | None
) -> tuple[str, ZephyrSDK]:
    """Resolve a `zephyr: framework: type:` value to (type_name, ZephyrSDK).

    None (framework: type: omitted) resolves to the variant's own default sdk/sdk_name.
    Raises KeyError for an unknown type -- callers validate against
    [variant.sdk_name, *variant.alt_sdks] via cv.one_of() beforehand, so this should never
    actually miss in practice.
    """
    if framework_type is None or framework_type == variant.sdk_name:
        return variant.sdk_name, variant.sdk
    return framework_type, variant.alt_sdks[framework_type]


def _sdk_default_version(variant: ZephyrVariant, sdk_name: str, sdk: ZephyrSDK) -> str:
    # default_version_override only applies to the variant's own default sdk -- an alt
    # sdk's version window is entirely its own (e.g. nrf52's alt "zephyr" sdk is just
    # MAINLINE, unmodified).
    if sdk_name == variant.sdk_name:
        return variant.default_version_override or sdk.default_version
    return sdk.default_version


def _sdk_min_version(
    variant: ZephyrVariant, sdk_name: str, sdk: ZephyrSDK
) -> cv.Version:
    if sdk_name == variant.sdk_name:
        return variant.min_version_override or sdk.min_version
    return sdk.min_version


def qualify_board(
    variant: ZephyrVariant, board: str, qualifier: str | None = None
) -> str:
    """Expand a bare board name into its full west target string. Already-qualified input
    is left untouched.

    qualifier overrides variant.qualifier for variants where the qualifier segment isn't
    a fixed hardware fact (e.g. RP2040's board/soc/mcuboot vs. board/soc split depends on
    a user config choice, not the chip itself) -- None (the default) falls back to
    variant.qualifier, unchanged behavior for every other variant.
    """
    if "/" in board or variant.soc is None:
        return board
    qualified = f"{board}/{variant.soc}"
    qualifier = variant.qualifier if qualifier is None else qualifier
    if qualifier:
        qualified += f"/{qualifier}"
    return qualified


def resolve_framework_version(
    variant: ZephyrVariant, variant_name: str, config: ConfigType, requires_reason: str
) -> tuple[str, cv.Version, str, ZephyrSDK]:
    """Resolve and validate `zephyr: framework:` for a variant's config_schema().

    Handles `type:` selection (the variant's default sdk, or one of its alt_sdks),
    `version:`/`source:` mutual exclusion, `source:` version discovery, and min_version
    validation (requires_reason explains why, e.g. "mainline ESP32-C6 support"); only
    warns (doesn't block) if the resolved version differs from the sdk's own
    default_version. Returns (version_str, parsed_version, sdk_name, sdk) for the caller
    to store via set_core_data().
    """
    framework = config[CONF_FRAMEWORK]
    type_names = [variant.sdk_name, *variant.alt_sdks]
    framework_type = framework.get(CONF_TYPE)
    if framework_type is not None and framework_type not in type_names:
        raise cv.Invalid(
            f"'{framework_type}' is not a valid framework type for {variant_name}. "
            f"Valid: {type_names!r}",
            [CONF_FRAMEWORK, CONF_TYPE],
        )
    sdk_name, sdk = resolve_sdk(variant, framework_type)
    default_version = _sdk_default_version(variant, sdk_name, sdk)
    min_version = _sdk_min_version(variant, sdk_name, sdk)

    if CONF_VERSION in framework and CONF_SOURCE in framework:
        raise cv.Invalid(
            "'version' and 'source' are mutually exclusive within 'framework:' -- "
            "'source' determines the version from the source itself.",
            [CONF_FRAMEWORK, CONF_SOURCE],
        )

    source = framework.get(CONF_SOURCE)
    if source is None and sdk.requires_source_reason is not None:
        raise cv.Invalid(
            f"framework: type: {sdk_name} has no usable default version -- "
            f"{sdk.requires_source_reason}. 'source:' is required.",
            [CONF_FRAMEWORK, CONF_TYPE],
        )

    if source is not None:
        from ..dts_fetch import (  # deferred: avoid import cycle (dts_fetch imports VARIANTS)
            resolve_sdk_source_version,
        )

        if source[CONF_TYPE] == TYPE_GIT and CONF_URL not in source:
            source[CONF_URL] = sdk.manifest_url
        with cv.prepend_path([CONF_FRAMEWORK, CONF_SOURCE]):
            version_str = resolve_sdk_source_version(source, framework[CONF_REFRESH])
    else:
        version_str = str(framework.get(CONF_VERSION, default_version))
        if version_str == VERSION_RECOMMENDED:
            version_str = default_version

    framework_ver = cv.Version.parse(version_str)
    if framework_ver < min_version:
        raise cv.Invalid(
            f"{variant_name} requires {sdk_name} >= {min_version} ({requires_reason}). "
            f"Got {version_str}.",
            [CONF_FRAMEWORK, CONF_VERSION],
        )
    if source is None and framework_ver != cv.Version.parse(default_version):
        _LOGGER.warning(
            "The selected %s version is not the recommended one (%s). "
            "If there are connectivity or build issues please remove the manual version.",
            sdk_name,
            default_version,
        )
    return version_str, framework_ver, sdk_name, sdk


def set_core_data(
    variant_name: str,
    board: str,
    bootloader: str,
    framework_ver: cv.Version,
    config: ConfigType,
    *,
    framework_type: str,
    sdk_source: ConfigType | None = None,
    prj_conf: dict | None = None,
    sysbuild_conf: dict | None = None,
    overlay: dict | None = None,
    overlay_builder: list | None = None,
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
    board_root: Path | None = None,
    shields: list[str] | None = None,
    shield_root: Path | None = None,
    snippet_root: Path | None = None,
    runner: str | None = None,
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
        sdk_source=sdk_source,
        bootloader=bootloader,
        variant=variant_name,
        family=VARIANTS[variant_name].family,
        framework_type=framework_type,
        prj_conf=prj_conf if prj_conf is not None else {},
        sysbuild_conf=sysbuild_conf if sysbuild_conf is not None else {},
        overlay=overlay if overlay is not None else {"": ""},
        overlay_builder=overlay_builder if overlay_builder is not None else [],
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
        west_version=config.get(CONF_WEST_VERSION),
        ninja_version=config.get(CONF_NINJA_VERSION),
        snippets=config.get(CONF_SNIPPETS, []),
        single_slot=config.get(CONF_SINGLE_SLOT, False),
        shields=shields if shields is not None else [],
        shield_root=shield_root,
        snippet_root=snippet_root,
        module_requests={},
        module_overrides={},
        blobs=[],
        runner=runner,
    )


MAINLINE: ZephyrSDK = ZephyrSDK(
    manifest_url="https://github.com/zephyrproject-rtos/zephyr",
    tools_subdir="sdk-zephyr",
)

# Nordic's ZBOSS Zigbee stack, split out of sdk-nrf into this separate add-on repo as of
# NCS 3.4.0; see https://github.com/esphomeplatformzephyr/esphome/issues/31. Additive to
# NCS (fetched alongside sdk-nrf, no boards/toolchain/version of its own) -- not a
# competing SDK root, so it's a module, not an alt_sdks entry. sdk-nrf and ncs-zigbee are
# versioned independently by Nordic (3.4.x vs 1.4.x, no formula between the two), hence
# the explicit per-pairing table rather than a single default_version.
NCS_ZIGBEE_TEMPLATE: ZephyrModuleTemplate = ZephyrModuleTemplate(
    name="ncs-zigbee",
    manifest_url="https://github.com/nrfconnect/ncs-zigbee",
    default_versions={(3, 4): "1.4.0"},
    min_version=cv.Version(1, 4, 0),
)

# nRF Connect SDK (NCS) -- Nordic's own vendor SDK, a manifest-of-manifests around a
# bundled Zephyr fork (nrfconnect/sdk-zephyr) rather than a single repo. boards/dts/
# for real Nordic silicon live in that bundled fork, not in the nrf manifest repo
# itself, and the fork's own checkout ref isn't a stable function of the nrf release
# version -- see resolve_boards_ref_via_manifest on ZephyrSDK. 3.4.0: Nordic's stated
# minimum for this integration; its bundled zephyr fork reports 4.4.0 (close to
# MAINLINE's own 4.4.1 baseline), though NCS cherry-picks fixes independently of
# upstream release numbers, so that number is not a reliable cross-SDK gating signal.
NCS: ZephyrSDK = ZephyrSDK(
    manifest_url="https://github.com/nrfconnect/sdk-nrf",
    boards_repo_url="https://github.com/nrfconnect/sdk-zephyr",
    tools_subdir="sdk-nrf",
    default_version="3.4.0",
    min_version=cv.Version(3, 4, 0),
    resolve_boards_ref_via_manifest=True,
    modules={"zigbee": NCS_ZIGBEE_TEMPLATE},
    west_project_name="nrf",
)

# Silicon Labs' "Simplicity SDK for Zephyr" -- same manifest-of-manifests shape as NCS:
# a vendor overlay repo (mcuboot/tf-psa-crypto overrides, hal_silabs, extra HAL modules)
# around a bundled Zephyr fork (SiliconLabsSoftware/zephyr) pinned by raw commit SHA in
# zephyr-silabs's own west.yml, not derivable from the vendor repo's version tag -- same
# reasoning as NCS's resolve_boards_ref_via_manifest. Real board/DTS support for chips
# not yet upstreamed to mainline Zephyr lives in that bundled fork.
SILABS: ZephyrSDK = ZephyrSDK(
    manifest_url="https://github.com/SiliconLabsSoftware/zephyr-silabs",
    boards_repo_url="https://github.com/SiliconLabsSoftware/zephyr",
    tools_subdir="sdk-silabs",
    default_version="2026.6.0",
    min_version=cv.Version(2026, 6, 0),
    resolve_boards_ref_via_manifest=True,
)

# Module names under this package that define a variant.
# Each module must export VARIANT_NAME and a ZephyrVariant instance named VARIANT.
_VARIANT_MODULES = [
    "esp32",
    "esp32_h2",
    "esp32_c6",
    "esp32_c5",
    "esp32_c3",
    "native_sim",
    "nrf52",
    "nrf54l15",
    "nrf54lm20a",
    "efr32mg24",
    "stm32l4",
    "stm32f4",
    "stm32wb55",
    "rp2040",
    "rp2350",
    "ra4m1",
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
