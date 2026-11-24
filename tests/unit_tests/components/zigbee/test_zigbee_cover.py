"""Tests for the zigbee cover validators."""

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
    KEY_ZIGBEE_EP_NO_NUM,
)
from esphome.components.zigbee.zigbee_ep_esp32 import create_ep
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ENDPOINT,
    CONF_ID,
    CONF_INTERNAL,
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
    # ZigbeeCover owns the lift percentage and ConfigStatus attributes
    assert not any(a.get(CONNECT) for a in cluster[CONF_ATTRIBUTES])
    assert KEY_ZIGBEE_ATTRIBUTES not in config
    assert config[KEY_ZIGBEE_EP] == {CONF_NUMBER: 3}
    assert config[CONF_ZIGBEE_COVER].type is ZigbeeCover
    assert config[CONF_ZIGBEE_ID].type is ZigbeeComponent
    values = {a[CONF_ATTRIBUTE_ID]: a[CONF_VALUE] for a in cluster[CONF_ATTRIBUTES]}
    # mode 0, not the maintenance default
    assert values[MODE_ATTRIBUTE] == 0x00


@pytest.mark.parametrize(
    ("device_class", "covering_type"),
    [
        pytest.param(None, 0x00, id="none-rollershade"),
        pytest.param("awning", 0x05, id="awning"),
        pytest.param("curtain", 0x04, id="curtain-drapery"),
        pytest.param("shutter", 0x00, id="shutter-lift-not-tilt-only"),
        pytest.param("blind", 0x00, id="blind-lift-only"),
        pytest.param("garage", 0x00, id="unmapped-rollershade"),
    ],
)
def test_cover_window_covering_type_from_device_class(
    device_class: str | None, covering_type: int
) -> None:
    _configure()
    extra = {CONF_ENDPOINT: 3}
    if device_class is not None:
        extra[CONF_DEVICE_CLASS] = device_class

    zigbee.validate_cover(_cover(**extra))

    (cluster,) = CORE.data[KEY_ZIGBEE][KEY_ZIGBEE_EP][3][CONF_CLUSTERS]
    values = {a[CONF_ATTRIBUTE_ID]: a[CONF_VALUE] for a in cluster[CONF_ATTRIBUTES]}
    assert values[WINDOW_COVERING_TYPE_ATTRIBUTE] == covering_type


def test_cover_without_endpoint_is_queued_for_numbering() -> None:
    _configure()

    config = zigbee.validate_cover(_cover())

    assert len(CORE.data[KEY_ZIGBEE][KEY_ZIGBEE_EP_NO_NUM]) == 1
    assert KEY_ZIGBEE_EP not in CORE.data[KEY_ZIGBEE]
    create_ep(False)
    assert config[KEY_ZIGBEE_EP] == {CONF_NUMBER: 1}


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


def test_nrf52_cover_rejects_endpoint() -> None:
    _configure(PLATFORM_NRF52)

    with pytest.raises(cv.Invalid, match="only supported on ESP32"):
        zigbee.validate_cover(_cover(**{CONF_ENDPOINT: 3}))
