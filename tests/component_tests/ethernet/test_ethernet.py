"""Tests for the ethernet final-validation coexistence gate."""

import pytest
from voluptuous import Invalid

from esphome.components.ethernet import _final_validate
from esphome.components.network import _validate_priority_list
from esphome.const import CONF_PRIORITY
import esphome.final_validate as fv


@pytest.fixture(autouse=True)
def _reset_full_config():
    """Reset fv.full_config so each test starts with a clean slate."""
    token = fv.full_config.set({})
    yield
    fv.full_config.reset(token)


def test_rejects_wifi_and_ethernet_without_priority() -> None:
    """Wi-Fi + ethernet without a network: priority: list must be rejected."""
    fv.full_config.set({"wifi": {}, "ethernet": {}})
    with pytest.raises(Invalid, match="cannot be used together with component wifi"):
        _final_validate({})


def test_rejects_wifi_and_ethernet_with_incomplete_priority() -> None:
    """A priority list missing an interface is rejected and names what's missing."""
    fv.full_config.set(
        {
            "wifi": {},
            "ethernet": {},
            "network": {CONF_PRIORITY: _validate_priority_list(["ethernet"])},
        }
    )
    with pytest.raises(Invalid, match=r"must.*list both interfaces; missing: wifi"):
        _final_validate({})
