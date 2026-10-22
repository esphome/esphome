"""Tests for the ethernet final-validation coexistence gate and schema bounds."""

import pytest
from voluptuous import Invalid

from esphome import config_validation as cv
from esphome.components.esp32 import (
    KEY_BOARD,
    KEY_IDF_VERSION,
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32S3,
)
from esphome.components.ethernet import (
    CONF_CLOCK_SPEED,
    CONFIG_SCHEMA,
    _final_validate_wifi,
)
from esphome.components.network import _validate_priority_list
from esphome.const import CONF_AP, CONF_NETWORKS, CONF_PRIORITY, PlatformFramework
from esphome.core import CORE
import esphome.final_validate as fv

from ..types import SetCoreConfigCallable

_CH390_CONFIG = {
    "type": "CH390",
    "clk_pin": 47,
    "mosi_pin": 48,
    "miso_pin": 14,
    "cs_pin": 21,
}


_WIFI_STA_CONFIG = {CONF_NETWORKS: [{"ssid": "MySSID", "password": "password1"}]}
_WIFI_AP_CONFIG = {CONF_AP: {"ssid": "espap", "password": "mypassword"}}


@pytest.mark.parametrize(
    ("wifi", "priority", "extra", "match"),
    [
        (_WIFI_STA_CONFIG, None, {}, "missing: ethernet, wifi"),
        (_WIFI_STA_CONFIG, ["ethernet"], {}, "missing: wifi"),
        (_WIFI_STA_CONFIG, ["wifi", "ethernet"], {}, None),
        (_WIFI_AP_CONFIG, None, {}, None),
        # A station can still appear at run time: DPP or a provisioner saves credentials
        ({"dpp": {}}, None, {}, "missing: ethernet, wifi"),
        ({"dpp": {}, **_WIFI_AP_CONFIG}, None, {}, "missing: ethernet, wifi"),
        (_WIFI_AP_CONFIG, None, {"captive_portal": {}}, "missing: ethernet, wifi"),
        (_WIFI_AP_CONFIG, None, {"improv_serial": {}}, "missing: ethernet, wifi"),
    ],
)
def test_wifi_station_needs_a_full_priority_list_but_an_access_point_does_not(
    wifi: dict, priority: list[str] | None, extra: dict, match: str | None
) -> None:
    """A station, configured or provisioned later, competes with ethernet; an access point does not."""
    full = {"wifi": wifi, "ethernet": {}, **extra}
    if priority:
        full["network"] = {CONF_PRIORITY: _validate_priority_list(priority)}
    fv.full_config.set(full)
    if match is None:
        _final_validate_wifi({})
    else:
        with pytest.raises(Invalid, match=match):
            _final_validate_wifi({})


@pytest.mark.parametrize("clock_speed", ["26.67MHz", "72MHz"])
def test_ch390_accepts_clock_speed_up_to_the_datasheet_maximum(
    set_core_config: SetCoreConfigCallable, clock_speed: str
) -> None:
    """CH390 SCK is rated to 72MHz, so the schema must accept the whole range."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={
            KEY_BOARD: "esp32-s3-devkitc-1",
            KEY_VARIANT: VARIANT_ESP32S3,
            KEY_IDF_VERSION: cv.Version(5, 3, 2),
        },
    )
    # _validate derives use_address from the node name, which has no default here.
    CORE.name = "ch390-test"
    config = CONFIG_SCHEMA({**_CH390_CONFIG, CONF_CLOCK_SPEED: clock_speed})
    assert config[CONF_CLOCK_SPEED] == cv.frequency(clock_speed)


def test_ch390_rejects_clock_speed_above_the_datasheet_maximum(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The shared 80MHz ceiling is out of spec for this part."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={
            KEY_BOARD: "esp32-s3-devkitc-1",
            KEY_VARIANT: VARIANT_ESP32S3,
            KEY_IDF_VERSION: cv.Version(5, 3, 2),
        },
    )
    # _validate derives use_address from the node name, which has no default here.
    CORE.name = "ch390-test"
    with pytest.raises(Invalid, match="value must be at most 72000000"):
        CONFIG_SCHEMA({**_CH390_CONFIG, CONF_CLOCK_SPEED: "80MHz"})


_LAN8720_CONFIG = {"type": "LAN8720", "mdc_pin": 23, "mdio_pin": 18}


def _set_esp32_core(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={
            KEY_BOARD: "esp32dev",
            KEY_VARIANT: VARIANT_ESP32,
            KEY_IDF_VERSION: cv.Version(5, 3, 2),
        },
    )
    CORE.name = "rmii-test"


def test_rmii_rejects_removed_clk_mode(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The removed clk_mode option points at the clk: replacement."""
    _set_esp32_core(set_core_config)
    with pytest.raises(Invalid, match="GPIO17_OUT -> mode: CLK_OUT, pin: GPIO17"):
        CONFIG_SCHEMA(
            {
                **_LAN8720_CONFIG,
                "clk_mode": "GPIO17_OUT",
                "clk": {"mode": "CLK_OUT", "pin": 17},
            }
        )


def test_rmii_requires_clk(set_core_config: SetCoreConfigCallable) -> None:
    """An RMII PHY has no default clock, so clk must be given."""
    _set_esp32_core(set_core_config)
    with pytest.raises(Invalid, match=r"required key not provided @ data\['clk'\]"):
        CONFIG_SCHEMA(_LAN8720_CONFIG)
