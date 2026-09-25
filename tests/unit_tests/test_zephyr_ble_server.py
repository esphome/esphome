"""Unit tests for the zephyr_ble_server config validation.

The component compile test (tests/components/zephyr_ble_server/test.nrf52-xiao-ble.yaml)
covers the mtu: 247 path and validate.nrf52-xiao-ble.yaml covers the default-mtu path,
but neither can assert that a config is *rejected* -- `esphome config` expects success.
The SMP MTU-floor rejection branch is therefore covered here instead.
"""

import pytest

from esphome.components.zephyr_ble_server import (
    CONF_MTU,
    CONF_ON_NUMERIC_COMPARISON_REQUEST,
    DEFAULT_MTU,
    SMP_MIN_MTU,
    _validate_mtu_for_smp,
)
from esphome.config_validation import Invalid


def test_validate_mtu_for_smp__rejects_sub_floor_mtu_with_pairing():
    """An mtu between the default and the SMP floor is rejected when pairing is set."""
    config = {
        CONF_MTU: SMP_MIN_MTU - 15,
        CONF_ON_NUMERIC_COMPARISON_REQUEST: [{}],
    }
    with pytest.raises(Invalid, match=str(SMP_MIN_MTU)):
        _validate_mtu_for_smp(config)


def test_validate_mtu_for_smp__allows_default_mtu_with_pairing():
    """The default mtu is allowed with pairing; Zephyr applies the SMP floor itself."""
    config = {
        CONF_MTU: DEFAULT_MTU,
        CONF_ON_NUMERIC_COMPARISON_REQUEST: [{}],
    }
    assert _validate_mtu_for_smp(config) is config


def test_validate_mtu_for_smp__allows_floor_mtu_with_pairing():
    """An mtu at or above the SMP floor is allowed with pairing."""
    config = {
        CONF_MTU: SMP_MIN_MTU,
        CONF_ON_NUMERIC_COMPARISON_REQUEST: [{}],
    }
    assert _validate_mtu_for_smp(config) is config


def test_validate_mtu_for_smp__ignores_sub_floor_mtu_without_pairing():
    """Without pairing the SMP floor does not apply, so a low mtu is accepted."""
    config = {CONF_MTU: SMP_MIN_MTU - 15}
    assert _validate_mtu_for_smp(config) is config
