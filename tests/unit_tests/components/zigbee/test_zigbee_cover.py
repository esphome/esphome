"""Tests for the zigbee cover validators."""

import copy

import pytest

from esphome.components import zigbee
from esphome.components.zigbee.const import (
    CONF_ZIGBEE_COVER,
    CONF_ZIGBEE_ID,
    KEY_ZIGBEE,
    ZigbeeComponent,
    ZigbeeCover,
)
from esphome.components.zigbee.const_esp32 import (
    CONF_ATTRIBUTE_ID,
    CONF_ATTRIBUTES,
    CONF_CLUSTERS,
    CONNECT,
    DEVICE_TYPE,
    KEY_ZIGBEE_ATTRIBUTES,
    KEY_ZIGBEE_EP,
)
from esphome.components.zigbee.zigbee_ep_esp32 import (
    ANALOG_OUTPUT_EP,
    BINARY_OUTPUT_EP,
    add_ep,
    create_ep,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ENDPOINT,
    CONF_ID,
    CONF_NAME,
    CONF_NUMBER,
    CONF_REPORT,
    CONF_VALUE,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_NRF52,
)
from esphome.core import CORE
from esphome.types import ConfigType

WINDOW_COVERING_TYPE_ATTRIBUTE = 0x0
MODE_ATTRIBUTE = 0x17


def _configure(platform: str = PLATFORM_ESP32) -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: platform}
    CORE.loaded_integrations.add("zigbee")


def _cover(**extra: object) -> ConfigType:
    return {CONF_NAME: "Blind", **extra}


def test_cover_with_endpoint_gets_window_covering_cluster() -> None:
    _configure()

    config = zigbee.validate_cover(_cover(**{CONF_ENDPOINT: 3, CONF_REPORT: "force"}))

    endpoint = CORE.data[KEY_ZIGBEE][KEY_ZIGBEE_EP][3]
    assert endpoint[DEVICE_TYPE] == "WINDOW_COVERING"
    (cluster,) = endpoint[CONF_CLUSTERS]
    assert cluster[CONF_ID] == "WINDOW_COVERING"
    # ZigbeeCover owns the lift percentage and ConfigStatus attributes
    assert not any(a.get(CONNECT) for a in cluster[CONF_ATTRIBUTES])
    assert KEY_ZIGBEE_ATTRIBUTES not in config
    assert config[KEY_ZIGBEE_EP] == {CONF_NUMBER: 3}
    assert config[CONF_ZIGBEE_COVER].type is ZigbeeCover
    assert config[CONF_ZIGBEE_ID].type is ZigbeeComponent
    values = {a[CONF_ATTRIBUTE_ID]: a[CONF_VALUE] for a in cluster[CONF_ATTRIBUTES]}
    # mode 0, not the maintenance default
    assert values[MODE_ATTRIBUTE] == 0x00
    # ZigbeeCover sets the type at setup, after a copy inherited its device class
    assert WINDOW_COVERING_TYPE_ATTRIBUTE not in values


def test_covers_do_not_share_an_endpoint() -> None:
    """Both covers need a window covering server cluster, so they cannot be merged."""
    _configure()

    zigbee.validate_cover(_cover())
    zigbee.validate_cover(_cover(**{CONF_NAME: "Shutter"}))
    create_ep(False)

    endpoints = CORE.data[KEY_ZIGBEE][KEY_ZIGBEE_EP]
    assert sorted(endpoints) == [1, 2]
    assert {ep[DEVICE_TYPE] for ep in endpoints.values()} == {"WINDOW_COVERING"}


def test_auto_numbered_cover_gets_an_endpoint_of_its_own() -> None:
    """Entities added before or after the cover must not join its endpoint."""
    _configure()

    add_ep(copy.deepcopy(BINARY_OUTPUT_EP), None, None)
    config = zigbee.validate_cover(_cover())
    add_ep(copy.deepcopy(ANALOG_OUTPUT_EP), None, None)
    create_ep(False)

    endpoints = CORE.data[KEY_ZIGBEE][KEY_ZIGBEE_EP]
    assert sorted(endpoints) == [1, 2]
    cover_ep = endpoints[config[KEY_ZIGBEE_EP][CONF_NUMBER]]
    assert cover_ep[DEVICE_TYPE] == "WINDOW_COVERING"
    assert [cl[CONF_ID] for cl in cover_ep[CONF_CLUSTERS]] == ["WINDOW_COVERING"]
    (other_ep,) = (ep for ep in endpoints.values() if ep is not cover_ep)
    assert [cl[CONF_ID] for cl in other_ep[CONF_CLUSTERS]] == [
        "BINARY_OUTPUT",
        "ANALOG_OUTPUT",
    ]


def test_cover_with_explicit_endpoint_still_shares_it() -> None:
    _configure()

    add_ep(copy.deepcopy(BINARY_OUTPUT_EP), 3, None)
    zigbee.validate_cover(_cover(**{CONF_ENDPOINT: 3}))
    create_ep(False)

    endpoints = CORE.data[KEY_ZIGBEE][KEY_ZIGBEE_EP]
    assert sorted(endpoints) == [3]
    assert [cl[CONF_ID] for cl in endpoints[3][CONF_CLUSTERS]] == [
        "BINARY_OUTPUT",
        "WINDOW_COVERING",
    ]


def test_nrf52_cover_rejects_endpoint() -> None:
    _configure(PLATFORM_NRF52)

    config = _cover()
    assert zigbee.validate_cover(config) == config
    assert KEY_ZIGBEE not in CORE.data
    with pytest.raises(cv.Invalid, match="only supported on ESP32"):
        zigbee.validate_cover(_cover(**{CONF_ENDPOINT: 3}))
