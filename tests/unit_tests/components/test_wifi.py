"""Tests for WiFi component public helpers."""

from ipaddress import IPv4Address

import pytest

from esphome.components.esp32 import const
from esphome.components.network import (
    KEY_HIGH_PERFORMANCE_NETWORKING,
    KEY_HIGH_PERFORMANCE_WIFI,
)
from esphome.components.wifi import (
    CONF_AP_MODE,
    CONF_AP_TIMEOUT,
    CONF_DNS_SERVERS,
    _validate_dpp_device_info,
    check_placeholder_credentials,
    final_validate as wifi_final_validate,
    has_native_wifi,
    variant_has_wifi,
    wifi_network_ap,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_AP,
    CONF_DNS1,
    CONF_MANUAL_IP,
    CONF_NETWORKS,
    CONF_PASSWORD,
    CONF_SSID,
    CONF_WIFI,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
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


@pytest.mark.parametrize("info", ["ESPHome bench", "a\tb", " x"])
def test_dpp_device_info_rejects_whitespace(info: str) -> None:
    """The supplicant reads the text up to the first space."""
    with pytest.raises(cv.Invalid, match="whitespace"):
        _validate_dpp_device_info(info)
    assert _validate_dpp_device_info("bench-01") == "bench-01"


@pytest.fixture
def wisp_core(monkeypatch: pytest.MonkeyPatch, request: pytest.FixtureRequest) -> None:
    """An ESP32 build; the parameter overrides the framework or the ESP-IDF version."""
    params = getattr(request, "param", {})
    monkeypatch.setitem(
        CORE.data,
        KEY_CORE,
        {
            KEY_TARGET_PLATFORM: Platform.ESP32,
            KEY_TARGET_FRAMEWORK: params.get("framework", "esp-idf"),
            KEY_FRAMEWORK_VERSION: params.get("idf", cv.Version(5, 4, 0)),
        },
    )


def _wisp_ap(**overrides: object) -> dict:
    return {CONF_AP_MODE: "WISP", CONF_PASSWORD: "wisp12345678", **overrides}


def _final_validate(config: dict, full_extra: dict | None = None) -> None:
    token = fv.full_config.set({CONF_WIFI: config, **(full_extra or {})})
    try:
        wifi_final_validate(config)
    finally:
        fv.full_config.reset(token)


@pytest.mark.usefixtures("wisp_core")
@pytest.mark.parametrize(
    "ap", [{CONF_AP_MODE: "WISP"}, _wisp_ap(**{CONF_PASSWORD: ""})]
)
def test_wisp_requires_ap_password(ap: dict) -> None:
    """WISP must not route upstream access through an open access point."""
    with pytest.raises(cv.Invalid, match="password"):
        wifi_network_ap(ap)


@pytest.mark.usefixtures("wisp_core")
def test_wisp_accepts_ap_password() -> None:
    assert wifi_network_ap(_wisp_ap())[CONF_AP_MODE] == "WISP"


@pytest.mark.usefixtures("wisp_core")
def test_wisp_rejects_zero_ap_timeout() -> None:
    """Zero disables the fallback AP; the bridge would retry a failed start on every loop pass."""
    with pytest.raises(cv.Invalid, match="ap_timeout"):
        wifi_network_ap(_wisp_ap(**{CONF_AP_TIMEOUT: "0s"}))
    wifi_network_ap(_wisp_ap(**{CONF_AP_TIMEOUT: "30s"}))


@pytest.mark.usefixtures("wisp_core")
def test_dns_servers_reject_unspecified_address() -> None:
    """0.0.0.0 would leave the clients with the access point as their resolver."""
    with pytest.raises(cv.Invalid, match="0.0.0.0"):
        wifi_network_ap(_wisp_ap(dns_servers=["0.0.0.0"]))


@pytest.mark.usefixtures("wisp_core")
def test_dns_servers_require_wisp() -> None:
    with pytest.raises(cv.Invalid, match="dns_servers requires ap_mode: WISP"):
        wifi_network_ap({CONF_DNS_SERVERS: ["1.1.1.1"]})


@pytest.mark.usefixtures("wisp_core")
def test_dns_servers_limited_to_two() -> None:
    """The access point DHCP server has a main and a backup resolver."""
    with pytest.raises(cv.Invalid):
        wifi_network_ap(_wisp_ap(dns_servers=["1.1.1.1", "8.8.8.8", "9.9.9.9"]))


@pytest.mark.parametrize(
    ("wisp_core", "valid"),
    [({"idf": cv.Version(5, 5, 5)}, False), ({"idf": cv.Version(6, 0, 1)}, True)],
    indirect=["wisp_core"],
)
def test_second_dns_server_requires_idf_6(wisp_core: None, valid: bool) -> None:
    ap = _wisp_ap(dns_servers=["1.1.1.1", "8.8.8.8"])
    if valid:
        wifi_network_ap(ap)
    else:
        with pytest.raises(cv.Invalid, match="6.0.0"):
            wifi_network_ap(ap)


@pytest.mark.parametrize("wisp_core", [{"framework": "arduino"}], indirect=True)
def test_wisp_requires_esp_idf(wisp_core: None) -> None:
    """NAT is only available with the ESP-IDF framework."""
    with pytest.raises(cv.Invalid, match="incompatible"):
        wifi_network_ap(_wisp_ap())


@pytest.mark.parametrize("wisp_core", [{"idf": cv.Version(5, 0, 9)}], indirect=True)
def test_wisp_requires_idf_5_1(wisp_core: None) -> None:
    """esp_netif_napt_enable() is missing before ESP-IDF 5.1."""
    with pytest.raises(cv.Invalid, match="5.1.0"):
        wifi_network_ap(_wisp_ap())


@pytest.mark.usefixtures("wisp_core")
def test_wisp_requires_sta_network_or_ethernet() -> None:
    """WISP has nothing to share without an upstream network; ethernet is one too."""
    ap_only = {CONF_NETWORKS: [], CONF_AP: _wisp_ap()}
    with pytest.raises(cv.Invalid, match="ethernet as the upstream"):
        _final_validate(ap_only)
    _final_validate(ap_only, {"ethernet": {}})
    with pytest.raises(cv.Invalid, match="ethernet manual_ip needs dns1"):
        _final_validate(ap_only, {"ethernet": {CONF_MANUAL_IP: _manual_ip("0.0.0.0")}})


@pytest.mark.usefixtures("wisp_core")
def test_wisp_rejects_captive_portal() -> None:
    with pytest.raises(cv.Invalid, match="captive_portal"):
        _final_validate(
            {CONF_NETWORKS: [{}], CONF_AP: _wisp_ap()}, {"captive_portal": {}}
        )


def _manual_ip(dns1: str) -> dict:
    return {CONF_DNS1: IPv4Address(dns1)}


@pytest.mark.usefixtures("wisp_core")
def test_wisp_manual_ip_needs_dns1() -> None:
    """The bridge passes the station's resolvers on, which a static address must name."""
    with pytest.raises(cv.Invalid, match="dns1"):
        _final_validate(
            {
                CONF_NETWORKS: [{CONF_MANUAL_IP: _manual_ip("0.0.0.0")}],
                CONF_AP: _wisp_ap(),
            }
        )
    with pytest.raises(cv.Invalid, match="dns1"):
        _final_validate(
            {
                CONF_MANUAL_IP: _manual_ip("0.0.0.0"),
                CONF_NETWORKS: [{}],
                CONF_AP: _wisp_ap(),
            }
        )


@pytest.mark.usefixtures("wisp_core")
def test_wisp_manual_ip_with_dns1_or_dns_servers() -> None:
    _final_validate(
        {CONF_NETWORKS: [{CONF_MANUAL_IP: _manual_ip("1.1.1.1")}], CONF_AP: _wisp_ap()}
    )
    _final_validate(
        {
            CONF_NETWORKS: [{CONF_MANUAL_IP: _manual_ip("0.0.0.0")}],
            CONF_AP: _wisp_ap(**{CONF_DNS_SERVERS: [IPv4Address("1.1.1.1")]}),
        }
    )


@pytest.mark.usefixtures("wisp_core")
def test_wisp_final_validate_accepts_sta_and_ap() -> None:
    _final_validate({CONF_NETWORKS: [{}], CONF_AP: _wisp_ap()})
    # NAT needs the WiFi driver buffers, not the lwip TCP windows
    assert CORE.data.get(KEY_HIGH_PERFORMANCE_WIFI) is True
    assert not CORE.data.get(KEY_HIGH_PERFORMANCE_NETWORKING)
