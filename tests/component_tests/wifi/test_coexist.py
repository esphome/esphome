"""Tests for the AP+STA coexistence (``coexist:``) and NAPT (``napt:``) options.

Both options live in the ``wifi:`` ``ap:`` block and are ESP32/ESP-IDF only;
``coexist:`` keeps the access point up while the station stays connected, and
``napt:`` routes the AP subnet out through the station. Both are compile-time
choices: they emit the ``USE_WIFI_APSTA`` and ``USE_WIFI_AP_NAPT`` defines and no
C++ setters, so the firmware either has the mode or it does not.
"""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32
from esphome.components.wifi import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA
from esphome.const import KEY_FRAMEWORK_VERSION, Platform, PlatformFramework
from esphome.core import CORE
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable
from tests.component_tests.wifi import sdkconfig_option

_AP_DEFINE = "USE_WIFI_AP"
_APSTA_DEFINE = "USE_WIFI_APSTA"
_NAPT_DEFINE = "USE_WIFI_AP_NAPT"
_WISP_DEFINE = "USE_WIFI_AP_WISP"

_STA_NETWORK: ConfigType = {"ssid": "test", "password": "testtest"}


def _defines() -> set[str]:
    """Names of every define the config generated."""
    return {define.name for define in CORE.defines}


def _validate(wifi: ConfigType) -> None:
    """Run the wifi schema and its final validation, as loading a config does."""
    FINAL_VALIDATE_SCHEMA(CONFIG_SCHEMA({"use_address": "192.168.1.1", **wifi}))


@pytest.mark.parametrize("config_file", ["coexist.yaml", "coexist_legacy_ssid.yaml"])
def test_coexist_emits_apsta_without_napt(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
) -> None:
    """``coexist:`` keeps the AP up through WIFI_MODE_APSTA and no NAPT.

    Both the ``networks:`` list and the legacy single ``ssid:`` spelling count
    as a station for the option.
    """
    main_cpp = generate_main(component_config_path(config_file))
    defines = _defines()

    assert _AP_DEFINE in defines
    assert _APSTA_DEFINE in defines
    assert _NAPT_DEFINE not in defines
    # Roaming scans after a connect would take the AP down again.
    assert "set_post_connect_roaming(false);" in main_cpp
    assert sdkconfig_option("CONFIG_LWIP_IP_FORWARD") is None
    assert sdkconfig_option("CONFIG_LWIP_IPV4_NAPT") is None


def test_napt_emits_ip_forwarding(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """``napt:`` turns on lwip forwarding and the driver's NAPT hook."""
    generate_main(component_config_path("coexist_napt.yaml"))
    defines = _defines()

    assert _APSTA_DEFINE in defines
    assert _NAPT_DEFINE in defines
    assert sdkconfig_option("CONFIG_LWIP_IP_FORWARD") is True
    assert sdkconfig_option("CONFIG_LWIP_IPV4_NAPT") is True


def test_ap_defaults_stay_plain(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Without the options the AP stays a plain fallback access point."""
    main_cpp = generate_main(component_config_path("ap_default.yaml"))
    defines = _defines()

    assert _AP_DEFINE in defines
    assert _APSTA_DEFINE not in defines
    assert _NAPT_DEFINE not in defines
    # post_connect_roaming defaults to true in C++, so no setter is emitted.
    assert "set_post_connect_roaming(" not in main_cpp
    assert sdkconfig_option("CONFIG_LWIP_IP_FORWARD") is None
    assert sdkconfig_option("CONFIG_LWIP_IPV4_NAPT") is None
    # An AP is configured, so the esp32 reconciler keeps SoftAP and the DHCP
    # server in the build instead of trimming them.
    assert sdkconfig_option("CONFIG_ESP_WIFI_SOFTAP_SUPPORT") is None
    assert sdkconfig_option("CONFIG_LWIP_DHCPS") is None


def test_wisp_stays_on_its_own_path(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """``ap_mode: WISP`` routes the AP by itself, without the coexist options.

    The bridge shares its routed-AP code with ``napt:``, so neither may turn
    the other's defines on.
    """
    generate_main(component_config_path("wisp.yaml"))
    defines = _defines()

    assert _WISP_DEFINE in defines
    assert _APSTA_DEFINE not in defines
    assert _NAPT_DEFINE not in defines
    assert sdkconfig_option("CONFIG_LWIP_IP_FORWARD") is True
    assert sdkconfig_option("CONFIG_LWIP_IPV4_NAPT") is True


@pytest.mark.parametrize(
    ("platform_framework", "wifi", "match"),
    [
        # APSTA lives in the esp-idf driver only, so the ESP8266 and the ESP32
        # arduino framework are both out.
        (
            PlatformFramework.ESP8266_ARDUINO,
            {"networks": [_STA_NETWORK], "ap": {"ssid": "fallback", "coexist": True}},
            "only supported on ESP32",
        ),
        (
            PlatformFramework.ESP32_ARDUINO,
            {"networks": [_STA_NETWORK], "ap": {"ssid": "fallback", "coexist": True}},
            "only supported on ESP32",
        ),
        # Coexistence needs a station for the AP to fall back from.
        (
            PlatformFramework.ESP32_IDF,
            {
                "post_connect_roaming": False,
                "ap": {"ssid": "fallback", "coexist": True},
            },
            "requires at least one STA network",
        ),
        # The default post_connect_roaming scan would drop the AP again.
        (
            PlatformFramework.ESP32_IDF,
            {"networks": [_STA_NETWORK], "ap": {"ssid": "fallback", "coexist": True}},
            "incompatible with post_connect_roaming",
        ),
        # NAPT only makes sense on top of coexistence.
        (
            PlatformFramework.ESP32_IDF,
            {"networks": [_STA_NETWORK], "ap": {"ssid": "fallback", "napt": True}},
            r"requires AP\+STA coexistence",
        ),
        # NAPT carries the same platform restriction.
        (
            PlatformFramework.ESP8266_ARDUINO,
            {"networks": [_STA_NETWORK], "ap": {"ssid": "fallback", "napt": True}},
            "only supported on ESP32",
        ),
        # ap_timeout only gates the fallback AP; the coexist AP stays up.
        (
            PlatformFramework.ESP32_IDF,
            {
                "networks": [_STA_NETWORK],
                "post_connect_roaming": False,
                "ap": {"ssid": "fallback", "coexist": True, "ap_timeout": "60s"},
            },
            "no effect with",
        ),
        # The bridge keeps the AP up and routes its subnet on its own, so these
        # options would set up a second routed access point.
        (
            PlatformFramework.ESP32_IDF,
            {
                "networks": [_STA_NETWORK],
                "ap": {
                    "ssid": "fallback",
                    "password": "fallback123",
                    "ap_mode": "WISP",
                    "coexist": True,
                },
            },
            "keeps the access point up and routes its subnet",
        ),
        (
            PlatformFramework.ESP32_IDF,
            {
                "networks": [_STA_NETWORK],
                "ap": {
                    "ssid": "fallback",
                    "password": "fallback123",
                    "ap_mode": "WISP",
                    "napt": True,
                },
            },
            "keeps the access point up and routes its subnet",
        ),
    ],
)
def test_invalid_combinations(
    set_core_config: SetCoreConfigCallable,
    platform_framework: PlatformFramework,
    wifi: ConfigType,
    match: str,
) -> None:
    """Unsupported combinations are rejected while the config is loaded.

    The checks live in the schema's final validation, so they surface when the
    config is loaded rather than later, during code generation.
    """
    platform, _ = platform_framework.value
    set_core_config(
        platform_framework,
        # A loaded config always has a framework version; the WISP entries ask
        # for one because the bridge needs esp_netif_napt_enable().
        core_data={KEY_FRAMEWORK_VERSION: cv.Version(5, 5, 5)},
        # The esp32 split defaults are keyed by variant, so a schema that reads
        # one needs the variant set even before any esp32 config is loaded.
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32}
        if platform is Platform.ESP32
        else {},
    )
    with pytest.raises(cv.Invalid, match=match):
        _validate(wifi)


@pytest.mark.parametrize(
    ("framework_version", "valid"),
    [
        # esp_netif_napt_enable() is missing before ESP-IDF 5.1.
        (cv.Version(5, 0, 9), False),
        (cv.Version(5, 1, 0), True),
    ],
)
def test_napt_requires_idf_5_1(
    set_core_config: SetCoreConfigCallable,
    framework_version: cv.Version,
    valid: bool,
) -> None:
    """The routed AP needs ``esp_netif_napt_enable()``, missing before ESP-IDF 5.1."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        core_data={KEY_FRAMEWORK_VERSION: framework_version},
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    wifi = {
        "networks": [_STA_NETWORK],
        "post_connect_roaming": False,
        "ap": {"ssid": "fallback", "coexist": True, "napt": True},
    }
    if valid:
        _validate(wifi)
    else:
        with pytest.raises(cv.Invalid, match="5.1.0"):
            _validate(wifi)


def test_coexist_without_napt_accepts_idf_5_0(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Plain coexistence uses no NAPT hook, so IDF 5.0 still accepts it."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        core_data={KEY_FRAMEWORK_VERSION: cv.Version(5, 0, 9)},
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    _validate(
        {
            "networks": [_STA_NETWORK],
            "post_connect_roaming": False,
            "ap": {"ssid": "fallback", "coexist": True},
        }
    )
