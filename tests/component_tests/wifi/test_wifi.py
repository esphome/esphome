"""Config-validation tests for the wifi component."""

import pytest
from voluptuous import Invalid

from esphome import config_validation as cv
from esphome.components.esp32 import (
    KEY_BOARD,
    KEY_IDF_VERSION,
    KEY_VARIANT,
    VARIANT_ESP32,
)
from esphome.components.wifi import (
    CONF_AP_MODE,
    CONFIG_SCHEMA,
    _validate,
    final_validate,
)
from esphome.const import (
    CONF_AP,
    CONF_HIDDEN,
    CONF_NETWORKS,
    CONF_PASSWORD,
    CONF_SSID,
    CONF_USE_ADDRESS,
    KEY_FRAMEWORK_VERSION,
    PlatformFramework,
)
import esphome.final_validate as fv
from tests.component_tests.types import SetCoreConfigCallable


def test_hidden_folded_into_networks() -> None:
    """The shorthand 'hidden' option is moved into the generated networks entry."""
    config = _validate(
        {
            CONF_SSID: "MyHomeNetwork",
            CONF_PASSWORD: "password1",
            CONF_HIDDEN: True,
            CONF_USE_ADDRESS: "test.local",
        }
    )
    assert CONF_SSID not in config
    assert CONF_PASSWORD not in config
    assert CONF_HIDDEN not in config
    networks = config[CONF_NETWORKS]
    assert len(networks) == 1
    assert networks[0][CONF_SSID] == "MyHomeNetwork"
    assert networks[0][CONF_PASSWORD] == "password1"
    assert networks[0][CONF_HIDDEN] is True


def test_shorthand_without_hidden_has_no_hidden_key() -> None:
    """No 'hidden' key is set on the networks entry unless explicitly configured."""
    config = _validate(
        {
            CONF_SSID: "MyHomeNetwork",
            CONF_PASSWORD: "password1",
            CONF_USE_ADDRESS: "test.local",
        }
    )
    assert CONF_HIDDEN not in config[CONF_NETWORKS][0]


def test_hidden_without_ssid_rejected(set_core_config: SetCoreConfigCallable) -> None:
    """A 'hidden' option without an SSID has nothing to apply to and is rejected."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    with pytest.raises(Invalid, match="'hidden' requires 'ssid'"):
        CONFIG_SCHEMA({CONF_HIDDEN: True})


def test_wisp_bridge_needs_a_station_or_a_wired_upstream(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """An AP-only WISP bridge routes through ethernet or modem; without one it has no way out."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        core_data={KEY_FRAMEWORK_VERSION: cv.Version(5, 5, 1)},
        platform_data={
            KEY_BOARD: "esp32dev",
            KEY_VARIANT: VARIANT_ESP32,
            KEY_IDF_VERSION: cv.Version(5, 5, 1),
        },
    )
    config = CONFIG_SCHEMA(
        {
            CONF_USE_ADDRESS: "test.local",
            CONF_AP: {
                CONF_SSID: "bridge",
                CONF_PASSWORD: "wisp12345678",
                CONF_AP_MODE: "WISP",
            },
        }
    )
    with pytest.raises(Invalid, match="ethernet or modem as the upstream"):
        final_validate(config)
    fv.full_config.get()["ethernet"] = {}
    final_validate(config)
