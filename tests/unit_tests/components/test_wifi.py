"""Tests for WiFi configuration and ESP32 event handlers."""

from ipaddress import IPv4Address
from pathlib import Path
import re
import shutil
import subprocess

import pytest

from esphome.components.esp32 import KEY_ESP32, KEY_IDF_VERSION, const
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
            KEY_TARGET_PLATFORM: params.get("platform", Platform.ESP32),
            KEY_TARGET_FRAMEWORK: params.get("framework", "esp-idf"),
            KEY_FRAMEWORK_VERSION: params.get("idf", cv.Version(5, 4, 0)),
        },
    )
    # The ESP-IDF underneath, which an Arduino build maps from its own version; only an
    # ESP32 build has it
    if params.get("platform", Platform.ESP32) == Platform.ESP32:
        monkeypatch.setitem(
            CORE.data,
            KEY_ESP32,
            {KEY_IDF_VERSION: params.get("idf", cv.Version(5, 4, 0))},
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
    [
        ({"idf": cv.Version(5, 5, 5)}, False),
        ({"idf": cv.Version(6, 0, 1)}, True),
        ({"framework": "arduino", "idf": cv.Version(5, 5, 6)}, False),
        ({"framework": "arduino", "idf": cv.Version(6, 0, 1)}, True),
    ],
    indirect=["wisp_core"],
)
def test_second_dns_server_requires_idf_6(wisp_core: None, valid: bool) -> None:
    """The ESP-IDF underneath counts, so an Arduino build on ESP-IDF 6.0 gets it too."""
    ap = _wisp_ap(dns_servers=["1.1.1.1", "8.8.8.8"])
    if valid:
        wifi_network_ap(ap)
    else:
        with pytest.raises(cv.Invalid, match="ESP-IDF 6.0"):
            wifi_network_ap(ap)


@pytest.mark.parametrize(
    "wisp_core",
    [{"platform": Platform.ESP8266, "framework": "arduino"}],
    indirect=True,
)
def test_second_dns_server_elsewhere_is_a_config_error(wisp_core: None) -> None:
    """No ESP32 state to read a version from; the WISP platform check rejects it instead."""
    with pytest.raises(cv.Invalid, match="only available on"):
        wifi_network_ap(_wisp_ap(dns_servers=["1.1.1.1", "8.8.8.8"]))


@pytest.mark.parametrize(
    "wisp_core",
    [{"platform": Platform.ESP8266, "framework": "arduino"}],
    indirect=True,
)
def test_wisp_requires_esp32(wisp_core: None) -> None:
    """The bridge is part of the ESP32 backend; other platforms have no NAT."""
    with pytest.raises(cv.Invalid, match="only available on .*ESP32"):
        wifi_network_ap(_wisp_ap())


@pytest.mark.usefixtures("wisp_core")
def test_wisp_requires_sta_network_or_upstream() -> None:
    """WISP has nothing to share without an upstream network; ethernet and modem are one too."""
    ap_only = {CONF_NETWORKS: [], CONF_AP: _wisp_ap()}
    with pytest.raises(cv.Invalid, match="ethernet or modem as the upstream"):
        _final_validate(ap_only)
    _final_validate(ap_only, {"ethernet": {}})
    _final_validate(ap_only, {"modem": {}})
    with pytest.raises(cv.Invalid, match="ethernet manual_ip needs dns1"):
        _final_validate(ap_only, {"ethernet": {CONF_MANUAL_IP: _manual_ip("0.0.0.0")}})


@pytest.mark.usefixtures("wisp_core")
def test_wisp_accepts_captive_portal() -> None:
    """The portal is served on the bridge while the upstream is down."""
    _final_validate({CONF_NETWORKS: [{}], CONF_AP: _wisp_ap()}, {"captive_portal": {}})


@pytest.mark.usefixtures("wisp_core")
def test_wisp_portal_rejects_dns_servers() -> None:
    """Clients only find the portal while DHCP offers the AP's own address as resolver."""
    with pytest.raises(
        cv.Invalid, match="dns_servers cannot be combined with captive_portal"
    ):
        _final_validate(
            {
                CONF_NETWORKS: [{}],
                CONF_AP: _wisp_ap(**{CONF_DNS_SERVERS: [IPv4Address("1.1.1.1")]}),
            },
            {"captive_portal": {}},
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


@pytest.mark.parametrize("ipv6", [0, 1])
def test_esp32_roam_event_handlers(tmp_path: Path, ipv6: int) -> None:
    """Compile the production handlers against SDK stubs, without ESP-IDF."""
    if not (compiler := shutil.which("c++")):
        pytest.skip("C++ compiler unavailable")
    source = (
        Path(__file__).resolve().parents[3]
        / "esphome/components/wifi/wifi_component_esp_idf.cpp"
    ).read_text()
    start = source.index(
        "  } else if (data->event_base == WIFI_EVENT && data->event_id == WIFI_EVENT_STA_STOP)"
    )
    end = source.index(
        "  } else if (data->event_base == IP_EVENT && data->event_id == IP_EVENT_STA_LOST_IP)",
        start,
    )
    declaration = re.search(
        r"static std::optional<bssid_t>\s+s_expected_disconnect_bssid;", source
    )
    assert declaration is not None
    program = tmp_path / "roam.cpp"
    program.write_text(
        _ESP32_EVENT_STUBS
        + f"\n#define USE_NETWORK_IPV6 {ipv6}\n"
        + declaration[0]
        + "\nvoid WiFiComponent::wifi_process_event_(IDFWiFiEvent *data) { if (false) {\n"
        + source[start:end]
        + "\n}}\n"
        + _ESP32_EVENT_CASES
    )
    executable = tmp_path / "roam"
    subprocess.run(
        [compiler, "-std=c++20", str(program), "-o", str(executable)],
        check=True,
        capture_output=True,
        text=True,
        timeout=30,
    )
    subprocess.run(
        [str(executable)], check=True, capture_output=True, text=True, timeout=30
    )


_ESP32_EVENT_STUBS: str = r"""
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <optional>
#define ESP_LOGV(...) ((void)0)
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESPHOME_LOG_LEVEL 0
#define ESPHOME_LOG_LEVEL_INFO 3
#define ESPHOME_LOG_LEVEL_VERBOSE 5
#define USE_WIFI_CONNECT_STATE_LISTENERS
#define USE_WIFI_IP_STATE_LISTENERS
#define CONFIG_LWIP_IPV6_NUM_ADDRESSES 3
#define MAC_ADDRESS_PRETTY_BUFFER_SIZE 18
using bssid_t = std::array<uint8_t, 6>;
using esp_err_t = int;
enum { ESP_OK, WIFI_EVENT, IP_EVENT, WIFI_EVENT_STA_STOP, WIFI_EVENT_STA_AUTHMODE_CHANGE,
       WIFI_EVENT_STA_CONNECTED, WIFI_EVENT_STA_DISCONNECTED, IP_EVENT_STA_GOT_IP, IP_EVENT_GOT_IP6 };
constexpr int WIFI_REASON_ASSOC_LEAVE = 8, WIFI_REASON_NO_AP_FOUND = 201, WIFI_REASON_ROAMING = 207;
constexpr int WIFI_COMPONENT_STATE_STA_CONNECTED = 1;
struct esp_ip6_addr_t { uint32_t addr[4]; };
struct esp_netif_ip_info_t { struct { uint32_t addr{}; } ip; };
struct IDFWiFiEvent {
  int event_base{}, event_id{};
  struct {
    struct { uint8_t bssid[6]{}; uint16_t reason{}; uint8_t channel{}; }
        sta_connected, sta_disconnected, sta_authmode_change;
    struct { esp_netif_ip_info_t ip_info; } ip_got_ip;
    struct {} ip_got_ip6;
  } data;
};
static bool s_sta_started, s_sta_connected, s_sta_connect_error, s_sta_connect_not_found, s_sta_connecting;
static void *s_sta_netif;
static bool sdk_up = true;
static uint32_t sdk_ip = 42;
static int sdk_ip6 = 1, sdk_error = ESP_OK;
struct LwIPLock {};
bool esp_netif_is_netif_up(void *) { return sdk_up; }
esp_err_t esp_netif_get_ip_info(void *, esp_netif_ip_info_t *ip) { ip->ip.addr = sdk_ip; return sdk_error; }
int esp_netif_get_all_ip6(void *, esp_ip6_addr_t *) { return sdk_up ? sdk_ip6 : 0; }
esp_err_t esp_netif_create_ip6_linklocal(void *) { return ESP_OK; }
void format_mac_addr_upper(const uint8_t *, char *) {}
struct WiFiComponent {
  bool got_ipv4_address_{}, error_from_callback_{};
  uint8_t num_ipv6_addresses_{};
  int state_{}, disconnect_notifications{};
  struct { bool connect_state{}; } pending_;
  void update_connected_state_() {}
  void notify_disconnect_state_listeners_() { ++disconnect_notifications; }
  void notify_ip_state_listeners_() {}
  void handle_driver_roam_(const bssid_t &, uint8_t) {}
  void wifi_process_event_(IDFWiFiEvent *);
  void event(int base, int id, const bssid_t &bssid = {}, uint16_t reason = 0, uint32_t ip = 42) {
    IDFWiFiEvent e{};
    e.event_base = base; e.event_id = id;
    std::copy(bssid.begin(), bssid.end(), e.data.sta_connected.bssid);
    e.data.sta_disconnected = e.data.sta_connected;
    e.data.sta_disconnected.reason = reason;
    e.data.ip_got_ip.ip_info.ip.addr = ip;
    this->wifi_process_event_(&e);
  }
};
"""

_ESP32_EVENT_CASES: str = r"""
int main() {
  constexpr bssid_t old_ap{2, 0, 0, 0, 0, 1}, new_ap{2, 0, 0, 0, 0, 2};
  auto reset = [&] {
    s_expected_disconnect_bssid = old_ap;
    s_sta_connected = false; s_sta_connecting = true;
    s_sta_connect_error = false; s_sta_connect_not_found = false;
    return WiFiComponent{};
  };
  for (int failure : {0, 1, 2}) {
    auto w = reset();
    if (failure == 0) s_expected_disconnect_bssid.reset();
    w.event(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, failure == 1 ? new_ap : old_ap,
            failure == 2 ? 200 : WIFI_REASON_ASSOC_LEAVE);
    assert(w.error_from_callback_ && !s_expected_disconnect_bssid);
  }
  auto w = reset();
  w.got_ipv4_address_ = true; w.num_ipv6_addresses_ = 2;
  w.event(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, old_ap);
  assert(s_expected_disconnect_bssid && !s_sta_connected && !w.pending_.connect_state);
  w.event(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, old_ap, WIFI_REASON_ROAMING);
  assert(s_expected_disconnect_bssid && !w.error_from_callback_);
  w.event(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, old_ap, WIFI_REASON_ASSOC_LEAVE);
  assert(!s_expected_disconnect_bssid && s_sta_connecting && !w.error_from_callback_);
  assert(!w.got_ipv4_address_ && w.disconnect_notifications == 1);
#if USE_NETWORK_IPV6
  assert(w.num_ipv6_addresses_ == 0);
#endif
  w.event(IP_EVENT, IP_EVENT_STA_GOT_IP);
  assert(!w.got_ipv4_address_);
  w.event(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, new_ap);
  assert(s_sta_connected && w.pending_.connect_state);
  for (int invalid : {0, 1, 2, 3}) {
    sdk_up = invalid != 0; sdk_ip = invalid == 1 ? 0 : 42; sdk_error = invalid == 2 ? -1 : ESP_OK;
    w.event(IP_EVENT, IP_EVENT_STA_GOT_IP, {}, 0, invalid == 3 ? 43 : 42);
    assert(!w.got_ipv4_address_);
  }
  sdk_up = true; sdk_ip = 42; sdk_error = ESP_OK;
  w.event(IP_EVENT, IP_EVENT_STA_GOT_IP);
  assert(w.got_ipv4_address_);
#if USE_NETWORK_IPV6
  sdk_ip6 = 0;
  w.event(IP_EVENT, IP_EVENT_GOT_IP6);
  assert(w.num_ipv6_addresses_ == 0);
  sdk_ip6 = 1;
  w.event(IP_EVENT, IP_EVENT_GOT_IP6);
  w.event(IP_EVENT, IP_EVENT_GOT_IP6);
  assert(w.num_ipv6_addresses_ == 1);
#endif
  w.event(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, old_ap, WIFI_REASON_ASSOC_LEAVE);
  assert(w.error_from_callback_);  // The expectation was consumed only once.
  w = reset();
  w.event(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, new_ap);
  assert(!s_expected_disconnect_bssid);
  w = reset();
  w.event(WIFI_EVENT, WIFI_EVENT_STA_STOP);
  assert(!s_expected_disconnect_bssid);
}
"""
