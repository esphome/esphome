"""Tests for WiFi component public helpers."""

import pytest

from esphome.components.esp32 import const
from esphome.components.wifi import (
    CONF_AP_MODE,
    WIFI_NETWORK_AP,
    check_placeholder_credentials,
    final_validate as wifi_final_validate,
    has_native_wifi,
    variant_has_wifi,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_AP,
    CONF_NETWORKS,
    CONF_PASSWORD,
    CONF_SSID,
    CONF_WIFI,
    KEY_CORE,
    KEY_ESP32,
    KEY_IDF_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLACEHOLDER_WIFI_SSID,
    Platform,
)
from esphome.core import CORE, EsphomeError, Lambda
import esphome.final_validate as fv


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
    assert has_native_wifi(platform=Platform.RP2, board="rpipicow") is True
    assert has_native_wifi(platform=Platform.RP2, board="rpipico") is False


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
    assert has_native_wifi(platform=Platform.RP2) is True


def _wifi_config(
    *,
    networks: list[dict] | None = None,
    ap: dict | None = None,
) -> dict:
    """Build a minimal config dict matching the post-validation shape."""
    wifi: dict = {}
    if networks is not None:
        wifi[CONF_NETWORKS] = networks
    if ap is not None:
        wifi[CONF_AP] = ap
    return {CONF_WIFI: wifi}


def test_check_placeholder_credentials_passes_with_real_ssid() -> None:
    """A real SSID compiles without complaint."""
    config = _wifi_config(networks=[{CONF_SSID: "home_network"}])
    assert check_placeholder_credentials(config) is None


def test_check_placeholder_credentials_refuses_placeholder_ssid() -> None:
    """The placeholder SSID is rejected with an actionable message."""
    config = _wifi_config(networks=[{CONF_SSID: PLACEHOLDER_WIFI_SSID}])
    with pytest.raises(EsphomeError) as exc_info:
        check_placeholder_credentials(config)
    message = str(exc_info.value)
    assert "wifi.networks[0].ssid" in message
    assert "secrets.yaml" in message


def test_check_placeholder_credentials_refuses_placeholder_in_second_network() -> None:
    """Index reporting picks the placeholder out of a mixed network list."""
    config = _wifi_config(
        networks=[
            {CONF_SSID: "home_network"},
            {CONF_SSID: PLACEHOLDER_WIFI_SSID},
        ],
    )
    with pytest.raises(EsphomeError) as exc_info:
        check_placeholder_credentials(config)
    assert "wifi.networks[1].ssid" in str(exc_info.value)


def test_check_placeholder_credentials_refuses_placeholder_ap_ssid() -> None:
    """An AP using the placeholder broadcast name is also refused."""
    config = _wifi_config(ap={CONF_SSID: PLACEHOLDER_WIFI_SSID})
    with pytest.raises(EsphomeError) as exc_info:
        check_placeholder_credentials(config)
    assert "wifi.ap.ssid" in str(exc_info.value)


def test_check_placeholder_credentials_no_wifi_passes() -> None:
    """Ethernet-only / wifi-less configs skip the check entirely."""
    assert check_placeholder_credentials({}) is None


def test_check_placeholder_credentials_skips_template_ssid() -> None:
    """A templated (Lambda) SSID is not a string and is skipped."""
    config = _wifi_config(networks=[{CONF_SSID: Lambda('return "x";')}])
    assert check_placeholder_credentials(config) is None


def _set_wisp_core(
    monkeypatch: pytest.MonkeyPatch,
    framework: str = "esp-idf",
    idf: cv.Version | None = None,
) -> None:
    """Describe an ESP32 build for the WISP final validation tests."""
    monkeypatch.setitem(
        CORE.data,
        KEY_CORE,
        {
            KEY_TARGET_PLATFORM: Platform.ESP32,
            KEY_TARGET_FRAMEWORK: framework,
        },
    )
    monkeypatch.setitem(
        CORE.data, KEY_ESP32, {KEY_IDF_VERSION: idf or cv.Version(5, 4, 0)}
    )


def _validate_wisp(config: dict, full_extra: dict | None = None) -> None:
    token = fv.full_config.set({CONF_WIFI: config, **(full_extra or {})})
    try:
        wifi_final_validate(config)
    finally:
        fv.full_config.reset(token)


@pytest.mark.parametrize("password", [None, ""])
def test_wisp_mode_requires_ap_password(
    password: str | None, monkeypatch: pytest.MonkeyPatch
) -> None:
    """WISP must not route upstream access through an open access point."""
    _set_wisp_core(monkeypatch)
    ap_config = {CONF_AP_MODE: "WISP"}
    if password is not None:
        ap_config[CONF_PASSWORD] = password
    config = {CONF_NETWORKS: [{}], CONF_AP: ap_config}
    token = fv.full_config.set({CONF_WIFI: config})
    try:
        with pytest.raises(cv.Invalid, match="wifi.ap.password"):
            wifi_final_validate(config)
    finally:
        fv.full_config.reset(token)


def test_wisp_mode_accepts_ap_password(monkeypatch: pytest.MonkeyPatch) -> None:
    """A configured AP password keeps WISP mode available."""
    _set_wisp_core(monkeypatch)
    config = {
        CONF_NETWORKS: [{}],
        CONF_AP: {CONF_AP_MODE: "WISP", CONF_PASSWORD: "securepassword"},
    }
    token = fv.full_config.set({CONF_WIFI: config})
    try:
        wifi_final_validate(config)
    finally:
        fv.full_config.reset(token)


def test_wisp_mode_rejects_captive_portal(monkeypatch: pytest.MonkeyPatch) -> None:
    """The WISP access point never runs the captive portal."""
    _set_wisp_core(monkeypatch)
    config = {
        CONF_NETWORKS: [{}],
        CONF_AP: {CONF_AP_MODE: "WISP", CONF_PASSWORD: "securepassword"},
    }
    token = fv.full_config.set({CONF_WIFI: config, "captive_portal": {}})
    try:
        with pytest.raises(cv.Invalid, match="captive_portal"):
            wifi_final_validate(config)
    finally:
        fv.full_config.reset(token)


def test_wisp_mode_requires_sta_network(monkeypatch: pytest.MonkeyPatch) -> None:
    """WISP has nothing to share without an upstream network."""
    _set_wisp_core(monkeypatch)
    config = {
        CONF_NETWORKS: [],
        CONF_AP: {CONF_AP_MODE: "WISP", CONF_PASSWORD: "securepassword"},
    }
    with pytest.raises(cv.Invalid, match="at least one STA network"):
        _validate_wisp(config)


def test_wisp_mode_requires_esp_idf(monkeypatch: pytest.MonkeyPatch) -> None:
    """NAT is only available with the ESP-IDF framework."""
    _set_wisp_core(monkeypatch, framework="arduino")
    config = {
        CONF_NETWORKS: [{}],
        CONF_AP: {CONF_AP_MODE: "WISP", CONF_PASSWORD: "securepassword"},
    }
    with pytest.raises(cv.Invalid, match="only supported with ESP-IDF"):
        _validate_wisp(config)


def test_wisp_mode_requires_idf_5_1(monkeypatch: pytest.MonkeyPatch) -> None:
    """esp_netif_napt_enable() is missing before ESP-IDF 5.1."""
    _set_wisp_core(monkeypatch, idf=cv.Version(5, 0, 9))
    config = {
        CONF_NETWORKS: [{}],
        CONF_AP: {CONF_AP_MODE: "WISP", CONF_PASSWORD: "securepassword"},
    }
    with pytest.raises(cv.Invalid, match="ESP-IDF 5.1.0"):
        _validate_wisp(config)


def test_dns_servers_requires_wisp(monkeypatch: pytest.MonkeyPatch) -> None:
    """dns_servers only has meaning for the WISP access point."""
    _set_wisp_core(monkeypatch)
    config = {
        CONF_NETWORKS: [{}],
        CONF_AP: {CONF_AP_MODE: "FALLBACK", "dns_servers": ["1.1.1.1"]},
    }
    with pytest.raises(cv.Invalid, match="dns_servers requires ap_mode: WISP"):
        _validate_wisp(config)


def test_dns_servers_schema_limits_to_two_servers() -> None:
    """The SoftAP DHCP server has a main and a backup DNS server."""
    WIFI_NETWORK_AP({CONF_AP_MODE: "WISP", "dns_servers": ["1.1.1.1", "8.8.8.8"]})
    with pytest.raises(cv.Invalid):
        WIFI_NETWORK_AP(
            {CONF_AP_MODE: "WISP", "dns_servers": ["1.1.1.1", "8.8.8.8", "9.9.9.9"]}
        )


@pytest.mark.parametrize(
    ("idf", "valid"),
    [(cv.Version(5, 5, 5), False), (cv.Version(6, 0, 1), True)],
)
def test_second_dns_server_requires_idf_6(
    monkeypatch: pytest.MonkeyPatch, idf: cv.Version, valid: bool
) -> None:
    """A backup DNS server on the AP DHCP server needs ESP-IDF 6.0 or newer."""
    _set_wisp_core(monkeypatch, idf=idf)
    config = {
        CONF_NETWORKS: [{}],
        CONF_AP: {
            CONF_AP_MODE: "WISP",
            CONF_PASSWORD: "wisp12345678",
            "dns_servers": ["1.1.1.1", "8.8.8.8"],
        },
    }
    if valid:
        _validate_wisp(config)
    else:
        with pytest.raises(cv.Invalid, match="second server only with ESP-IDF 6"):
            _validate_wisp(config)
