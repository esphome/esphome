"""Config-validation tests for the wifi component."""

import pytest
from voluptuous import Invalid

from esphome.components.wifi import _validate
from esphome.const import (
    CONF_HIDDEN,
    CONF_NETWORKS,
    CONF_PASSWORD,
    CONF_SSID,
    CONF_USE_ADDRESS,
)


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


def test_hidden_without_ssid_rejected() -> None:
    """A 'hidden' option without an SSID has nothing to apply to and is rejected."""
    with pytest.raises(Invalid, match="hidden"):
        _validate({CONF_HIDDEN: True, CONF_USE_ADDRESS: "test.local"})
