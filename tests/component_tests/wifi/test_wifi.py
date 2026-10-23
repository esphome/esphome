"""Config-validation tests for the wifi component."""

import pytest
from voluptuous import Invalid

from esphome.components.wifi import CONFIG_SCHEMA, _validate, validate_variant
from esphome.const import (
    CONF_AP,
    CONF_HIDDEN,
    CONF_NETWORKS,
    CONF_PASSWORD,
    CONF_SSID,
    CONF_USE_ADDRESS,
    PlatformFramework,
)
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


def _set_zephyr_with_ap(set_core_config: SetCoreConfigCallable, variant: str) -> None:
    set_core_config(
        PlatformFramework.ZEPHYR_ZEPHYR,
        platform_data={"variant": variant, "board": "some_board"},
        full_config={"wifi": {CONF_AP: {}}},
    )


def test_ap_rejected_on_nrf70(set_core_config: SetCoreConfigCallable) -> None:
    """'ap:' is rejected on nRF70 Wi-Fi."""
    _set_zephyr_with_ap(set_core_config, "NRF5340")
    # The final-validate chain passes the previous step's None, not the wifi config.
    with pytest.raises(Invalid, match="'ap:' is not supported yet on the nRF70"):
        validate_variant(None)


def test_ap_allowed_on_zephyr_esp32(set_core_config: SetCoreConfigCallable) -> None:
    """'ap:' is accepted on Zephyr ESP32 variants."""
    _set_zephyr_with_ap(set_core_config, "ESP32C6")
    validate_variant(None)
