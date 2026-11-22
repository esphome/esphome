"""Endpoint ID reservation and allocation for nRF52."""

import pytest

from esphome.components.zigbee.endpoints_zephyr import EndpointIds
import esphome.config_validation as cv


def test_reserve_and_allocate() -> None:
    ids = EndpointIds()
    ids.reserve(2)
    with pytest.raises(cv.Invalid, match="Duplicate Zigbee endpoint: 2"):
        ids.reserve(2)
    assert [ids.allocate(), ids.allocate(), ids.allocate()] == [1, 3, 4]
