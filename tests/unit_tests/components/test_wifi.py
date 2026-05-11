"""Tests for WiFi component public helpers."""

import pytest

from esphome.components.esp32 import const
from esphome.components.wifi import has_native_wifi, variant_has_wifi
from esphome.const import Platform


@pytest.mark.parametrize(
    "variant",
    [
        # Upstream's canonical uppercase form.
        const.VARIANT_ESP32,
        const.VARIANT_ESP32S2,
        const.VARIANT_ESP32S3,
        const.VARIANT_ESP32C3,
        const.VARIANT_ESP32C6,
        # Lowercase form external callers (e.g. device-builder's
        # ``Esp32Variant`` StrEnum) surface.
        "esp32",
        "esp32s3",
        "esp32c3",
        # Mixed-case — defence in depth against future callers that
        # pull the value off some other serialisation.
        "Esp32",
    ],
)
def test_variant_has_wifi_for_native_phy_variants(variant: str) -> None:
    """Variants with a native WiFi PHY → True, case-insensitive."""
    assert variant_has_wifi(variant) is True


@pytest.mark.parametrize(
    "variant",
    [
        # Upstream's canonical uppercase form.
        const.VARIANT_ESP32H2,
        const.VARIANT_ESP32P4,
        # Lowercase form external callers (e.g. device-builder's
        # ``Esp32Variant`` StrEnum) surface.
        "esp32h2",
        "esp32p4",
        # Mixed-case — defence in depth against future callers that
        # pull the value off some other serialisation.
        "Esp32H2",
    ],
)
def test_variant_has_wifi_for_no_phy_variants(variant: str) -> None:
    """Variants that need ``esp32_hosted`` → False, case-insensitive."""
    assert variant_has_wifi(variant) is False


def test_has_native_wifi_dispatches_esp32_to_variant_check() -> None:
    """ESP32 platform routes through ``variant_has_wifi``."""
    assert (
        has_native_wifi(platform=Platform.ESP32, variant=const.VARIANT_ESP32C3) is True
    )
    assert (
        has_native_wifi(platform=Platform.ESP32, variant=const.VARIANT_ESP32H2) is False
    )


def test_has_native_wifi_esp32_variant_case_insensitive() -> None:
    """has_native_wifi accepts lowercase variant input.

    External callers (device-builder's wizard, etc.) may surface
    variant strings from their own enums that don't match upstream's
    uppercase convention. The dispatcher should classify them
    identically.
    """
    assert has_native_wifi(platform=Platform.ESP32, variant="esp32h2") is False
    assert has_native_wifi(platform=Platform.ESP32, variant="esp32c3") is True


def test_has_native_wifi_dispatches_rp2040_to_board_check() -> None:
    """RP2040 platform routes through ``rp2040.board_id_has_wifi``."""
    assert has_native_wifi(platform=Platform.RP2040, board="rpipicow") is True
    assert has_native_wifi(platform=Platform.RP2040, board="rpipico") is False


def test_has_native_wifi_returns_false_for_nrf52() -> None:
    """nRF52 family is BLE-only — no Wi-Fi PHY in the platform."""
    assert has_native_wifi(platform=Platform.NRF52) is False


def test_has_native_wifi_returns_false_for_host() -> None:
    """``host`` platform compiles ESPHome to a host binary — no radio at all."""
    assert has_native_wifi(platform=Platform.HOST) is False


def test_has_native_wifi_returns_false_for_unknown_platform() -> None:
    """Unknown platform string fails closed.

    A future platform added to ESPHome that's missed here returns
    False rather than silently emitting a ``wifi:`` block external
    tooling would have to compile and reject — fail-closed surfaces
    the gap as an obvious "needs wifi support added" signal.
    """
    assert has_native_wifi(platform="not-a-real-platform") is False


@pytest.mark.parametrize(
    "platform",
    [
        Platform.ESP8266,
        Platform.BK72XX,
        Platform.RTL87XX,
        Platform.LN882X,
        Platform.LIBRETINY_OLDSTYLE,
    ],
)
def test_has_native_wifi_returns_true_for_wifi_first_platforms(platform: str) -> None:
    """Catch-all Wi-Fi-first platforms → True regardless of board / variant."""
    assert has_native_wifi(platform=platform) is True


def test_has_native_wifi_esp32_without_variant_assumes_wifi() -> None:
    """ESP32 without a variant id falls open to True (the chip family default)."""
    assert has_native_wifi(platform=Platform.ESP32) is True


def test_has_native_wifi_rp2040_without_board_assumes_wifi() -> None:
    """RP2040 without a board id falls open to True (custom-board default)."""
    assert has_native_wifi(platform=Platform.RP2040) is True
