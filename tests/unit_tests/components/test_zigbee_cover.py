"""Tests for the zigbee cover validators."""

import pytest

from esphome.components import zigbee
from esphome.components.zigbee.const import KEY_ZIGBEE
from esphome.components.zigbee.const_esp32 import (
    CONF_ATTRIBUTE_ID,
    CONF_ATTRIBUTES,
    CONF_CLUSTERS,
    CONNECT,
    DEVICE_TYPE,
    KEY_ZIGBEE_ATTRIBUTES,
    KEY_ZIGBEE_EP,
    KEY_ZIGBEE_EP_NO_NUM,
)
from esphome.components.zigbee.zigbee_ep_esp32 import create_ep
from esphome.const import (
    CONF_ENDPOINT,
    CONF_ID,
    CONF_INTERNAL,
    CONF_NAME,
    CONF_REPORT,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_NRF52,
)
from esphome.core import CORE
from esphome.types import ConfigType

LIFT_PERCENTAGE_ATTRIBUTE = 0x8


def _configure(platform: str = PLATFORM_ESP32, *, zigbee_loaded: bool = True) -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: platform}
    if zigbee_loaded:
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
    connected = [a for a in cluster[CONF_ATTRIBUTES] if a.get(CONNECT)]
    assert [a[CONF_ATTRIBUTE_ID] for a in connected] == [LIFT_PERCENTAGE_ATTRIBUTE]
    assert connected[0][CONF_REPORT] == "force"
    assert config[KEY_ZIGBEE_ATTRIBUTES] == connected


def test_cover_without_endpoint_is_queued_for_numbering() -> None:
    _configure()

    zigbee.validate_cover(_cover())

    assert len(CORE.data[KEY_ZIGBEE][KEY_ZIGBEE_EP_NO_NUM]) == 1
    assert KEY_ZIGBEE_EP not in CORE.data[KEY_ZIGBEE]


def test_covers_do_not_share_an_endpoint() -> None:
    """Both covers need a window covering server cluster, so they cannot be merged."""
    _configure()

    zigbee.validate_cover(_cover())
    zigbee.validate_cover(_cover(**{CONF_NAME: "Shutter"}))
    create_ep(False)

    endpoints = CORE.data[KEY_ZIGBEE][KEY_ZIGBEE_EP]
    assert sorted(endpoints) == [1, 2]
    assert {ep[DEVICE_TYPE] for ep in endpoints.values()} == {"WINDOW_COVERING"}


@pytest.mark.parametrize(
    ("platform", "zigbee_loaded", "internal"),
    [
        pytest.param(PLATFORM_ESP32, False, False, id="zigbee-not-loaded"),
        pytest.param(PLATFORM_ESP32, True, True, id="internal"),
        pytest.param(PLATFORM_NRF52, True, False, id="nrf52"),
    ],
)
def test_cover_is_not_exposed(
    platform: str, zigbee_loaded: bool, internal: bool
) -> None:
    _configure(platform, zigbee_loaded=zigbee_loaded)
    config = _cover(**{CONF_INTERNAL: internal})

    assert zigbee.validate_cover(config) == config
    assert KEY_ZIGBEE not in CORE.data
