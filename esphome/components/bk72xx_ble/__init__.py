"""BK72xx BLE — BLE controller support for the BLE-5.x LibreTiny Beken chips.

The platform analog of esp32_ble / rp2040_ble: owns the Beken BDK BLE stack
bring-up and the controller BLE address. Consumers (bk72xx_ble_tracker) build
on this component and contain no SDK calls of their own.

Supported SoCs (BLE 5.x): BK7231N/BK7236 (BLE 5.1), BK7252N/BK7253 (BLE 5.2),
and any future BLE-5.x SoC. BK7238 (BLE 5.2) is blocked for now: with BLE
compiled in, the Beken SDK erases the bootloader flash sector at boot because
LibreTiny's partition table has no BLE bonding entry (esphome#18646,
libretiny-eu/libretiny#408). Known non-5.x families and BK7238 are rejected in
to_code. Unknown families are capability-checked at compile time via
`__has_include("app_ble.h")`, a header only on the BLE 5.x include path
(ble_api.h ships for every SoC, so it cannot be the probe). A non-5.x build
fails with a clear #error.

No framework patch is needed: the LibreTiny beken-72xx builder already compiles
and links the BLE 5.x stack (CFG_SUPPORT_BLE=1 + CFG_BLE_VERSION=BLE_VERSION_5_x;
prebuilt libble_<chip>.a per SoC). This component only calls into it via the
public ble_api.h.
"""

import logging

import esphome.codegen as cg
from esphome.components import libretiny
from esphome.components.libretiny.const import (
    FAMILY_BK7231N,
    FAMILY_BK7231Q,
    FAMILY_BK7231T,
    FAMILY_BK7238,
    FAMILY_BK7251,
)
import esphome.config_validation as cv
from esphome.const import CONF_ENABLE_ON_BOOT, CONF_ID
from esphome.core import EsphomeError
from esphome.types import ConfigType

DEPENDENCIES = ["bk72xx"]
CODEOWNERS = ["@Bl00d-B0b"]

_LOGGER = logging.getLogger(__name__)

bk72xx_ble_ns = cg.esphome_ns.namespace("bk72xx_ble")
BK72xxBLE = bk72xx_ble_ns.class_("BK72xxBLE", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BK72xxBLE),
        # Default off: on the single-core BK72xx, bringing the BLE stack up during
        # boot competes with the WiFi connection handshake. Consumers enable the
        # stack lazily on first use (e.g. the tracker's first scan start).
        cv.Optional(CONF_ENABLE_ON_BOOT, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


# Once per registered scan listener; sizes the controller's StaticVector
# listener storage.
request_scan_listener_slot = cg.slot_counter("BK72XX_BLE_SCAN_LISTENER_COUNT")


def _unsupported_family_message(family: str) -> str | None:
    if family in (FAMILY_BK7231T, FAMILY_BK7251):
        return (
            f"bk72xx_ble does not support {family}: this SoC has the Beken BLE 4.2 "
            "stack; a BLE 5.x SoC such as BK7231N or BK7238 is required"
        )
    if family == FAMILY_BK7231Q:
        return "bk72xx_ble does not support BK7231Q: this SoC has no BLE"
    if family == FAMILY_BK7238:
        return (
            "bk72xx_ble is disabled on BK7238: with BLE compiled in, the Beken SDK "
            "erases the bootloader flash sector at boot and the device can no longer "
            "start (see https://github.com/esphome/esphome/issues/18646); support "
            "returns once the LibreTiny partition table fix "
            "(libretiny-eu/libretiny#408) is released"
        )
    return None


def _final_validate(config: ConfigType) -> None:
    # Warn only: a hard error here would break the validate-only CI fixtures,
    # which run on a BLE 4.2 board. The hard error is raised at codegen.
    if msg := _unsupported_family_message(libretiny.get_libretiny_family()):
        _LOGGER.warning("%s (this configuration cannot compile)", msg)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    if msg := _unsupported_family_message(libretiny.get_libretiny_family()):
        raise EsphomeError(msg)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_enable_on_boot(config[CONF_ENABLE_ON_BOOT]))

    # Enable the BLE stack in the build (the '#h' maps to sys_config.h; the value
    # is a list). ESPHome's libretiny platform normally appends CFG_SUPPORT_BLE=0
    # on BK7231N/BK7238 (saves ~21KB RAM/~200KB Flash when BLE is unused) to this
    # SAME key — and add_platformio_option appends list values, it never replaces.
    # The platform therefore skips its disable when this component is configured,
    # so this =1 is the single CFG_SUPPORT_BLE define emitted.
    cg.add_platformio_option("custom_options.sys_config#h", ["CFG_SUPPORT_BLE=1"])

    # Pin the Beken BDK release the BLE 5.x stack is validated against. The
    # bundled 3.0.33 has an older BLE header/library layout — and with
    # CFG_SUPPORT_BLE=1 the SDK runs its BLE init unconditionally during boot
    # (the reason the libretiny platform sets =0 when BLE is unused), so a
    # mismatched BDK can crash the device before WiFi comes up regardless of
    # enable_on_boot. Pinning here makes a plain config build against the
    # validated BDK without any manual platformio_options.
    _LOGGER.warning(
        "bk72xx_ble builds with beken-bdk 3.0.78 instead of the platform's bundled "
        "default: the default's older BLE layout can crash the device at boot when "
        "BLE is compiled in"
    )
    cg.add_platformio_option("custom_versions.beken-bdk", "3.0.78")

    # The BDK exposes the controller's BLE address as `common_default_bdaddr` on
    # BK7231N, but NOT on BK7238 (its BLE stack has no such symbol; the address is
    # derived from the WiFi MAC instead — the BDK's own fallback). Tell the C++
    # which path is available so it doesn't reference a missing symbol.
    if libretiny.get_libretiny_family() == FAMILY_BK7231N:
        cg.add_define("BK72XX_BLE_HAS_COMMON_BDADDR")

    cg.add_define("USE_BK72XX_BLE")
