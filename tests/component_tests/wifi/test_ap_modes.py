"""Tests for the access point modes that keep the AP up: ``ALWAYS_ON`` and ``WISP``."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32
from esphome.components.wifi import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA
from esphome.const import KEY_FRAMEWORK_VERSION, Platform, PlatformFramework
from esphome.core import CORE
from esphome.types import ConfigType
from tests.component_tests.network import sdkconfig_option
from tests.component_tests.types import SetCoreConfigCallable

_PERSISTENT = "USE_WIFI_AP_PERSISTENT"
_WISP = "USE_WIFI_AP_WISP"
_WISP_PORTAL = "USE_WIFI_AP_WISP_PORTAL"

_STA_NETWORK: ConfigType = {"ssid": "test", "password": "testtest"}
_ALWAYS_ON_AP: ConfigType = {"ssid": "fallback", "ap_mode": "ALWAYS_ON"}


@pytest.fixture
def validate(
    set_core_config: SetCoreConfigCallable,
) -> Callable[..., None]:
    """Run the wifi schema and its final validation on a target, as loading a config does."""

    def _validate(
        wifi: ConfigType,
        platform_framework: PlatformFramework = PlatformFramework.ESP32_IDF,
        full_config: ConfigType | None = None,
    ) -> None:
        platform, _ = platform_framework.value
        set_core_config(
            platform_framework,
            core_data={KEY_FRAMEWORK_VERSION: cv.Version(5, 5, 5)},
            # The esp32 split defaults are keyed by variant
            platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32}
            if platform is Platform.ESP32
            else {},
            full_config=full_config,
        )
        FINAL_VALIDATE_SCHEMA(CONFIG_SCHEMA({"use_address": "192.168.1.1", **wifi}))

    return _validate


@pytest.mark.parametrize(
    ("config_file", "present", "absent", "napt"),
    [
        ("ap_default.yaml", set(), {_PERSISTENT, _WISP, _WISP_PORTAL}, None),
        ("always_on.yaml", {_PERSISTENT}, {_WISP, _WISP_PORTAL}, None),
        ("wisp.yaml", {_PERSISTENT, _WISP}, {_WISP_PORTAL}, True),
        ("wisp_portal.yaml", {_PERSISTENT, _WISP, _WISP_PORTAL}, set(), True),
    ],
)
def test_ap_mode_defines(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    present: set[str],
    absent: set[str],
    napt: bool | None,
) -> None:
    """Only WISP routes; the AP stays up in both modes, and a WISP portal gets its own define."""
    generate_main(component_config_path(config_file))
    defines = {define.name for define in CORE.defines}

    assert "USE_WIFI_AP" in defines
    assert present <= defines
    assert not absent & defines
    assert sdkconfig_option("CONFIG_LWIP_IP_FORWARD") is napt
    assert sdkconfig_option("CONFIG_LWIP_IPV4_NAPT") is napt


@pytest.mark.parametrize(
    ("platform_framework", "wifi", "match"),
    [
        (
            PlatformFramework.ESP8266_ARDUINO,
            {"networks": [_STA_NETWORK], "ap": _ALWAYS_ON_AP},
            "only available on",
        ),
        (
            PlatformFramework.ESP32_IDF,
            {"post_connect_roaming": False, "ap": _ALWAYS_ON_AP},
            "ALWAYS_ON needs a station network",
        ),
        (
            PlatformFramework.ESP32_IDF,
            {"networks": [_STA_NETWORK], "ap": _ALWAYS_ON_AP},
            "incompatible with post_connect_roaming",
        ),
        (
            PlatformFramework.ESP32_IDF,
            {
                "networks": [_STA_NETWORK],
                "post_connect_roaming": False,
                "ap": {**_ALWAYS_ON_AP, "ap_timeout": "0s"},
            },
            "ap_timeout is how long ap_mode: ALWAYS_ON waits",
        ),
        (
            PlatformFramework.ESP32_IDF,
            {
                "networks": [_STA_NETWORK],
                "post_connect_roaming": False,
                "ap": {**_ALWAYS_ON_AP, "dns_servers": ["1.1.1.1"]},
            },
            "dns_servers requires ap_mode: WISP",
        ),
    ],
)
def test_always_on_invalid(
    validate: Callable[..., None],
    platform_framework: PlatformFramework,
    wifi: ConfigType,
    match: str,
) -> None:
    with pytest.raises(cv.Invalid, match=match):
        validate(wifi, platform_framework)


@pytest.mark.parametrize(
    ("platform_framework", "wifi", "full_config"),
    [
        (
            PlatformFramework.ESP32_IDF,
            {
                "networks": [_STA_NETWORK],
                "post_connect_roaming": False,
                "ap": {**_ALWAYS_ON_AP, "ap_timeout": "30s"},
            },
            None,
        ),
        (
            PlatformFramework.ESP32_ARDUINO,
            {
                "networks": [_STA_NETWORK],
                "post_connect_roaming": False,
                "ap": _ALWAYS_ON_AP,
            },
            None,
        ),
        # Ethernet or a modem is the upstream; with no station nothing can roam
        (PlatformFramework.ESP32_IDF, {"ap": _ALWAYS_ON_AP}, {"ethernet": {}}),
        (PlatformFramework.ESP32_IDF, {"ap": _ALWAYS_ON_AP}, {"modem": {}}),
    ],
)
def test_always_on_valid(
    validate: Callable[..., None],
    platform_framework: PlatformFramework,
    wifi: ConfigType,
    full_config: ConfigType | None,
) -> None:
    validate(wifi, platform_framework, full_config)
