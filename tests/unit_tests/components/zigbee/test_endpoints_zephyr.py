"""Test consistency of compact endpoint slots and reserved wire IDs."""

import pytest

from esphome.components.zigbee.endpoints_zephyr import EndpointIds
import esphome.config_validation as cv


def test_resolve_rejects_conflicting_endpoint() -> None:
    ids = EndpointIds()
    ids.reserve(17)
    ids.reserve(239)
    assert ids.resolve(0, 17) == 17
    assert ids.resolve(0) == 17
    assert ids.resolve(0, 17) == 17
    with pytest.raises(cv.Invalid, match="already assigned"):
        ids.resolve(0, 239)
    assert ids.assigned == {0: 17}


def test_resolve_requires_reserved_explicit_endpoint() -> None:
    ids = EndpointIds()
    with pytest.raises(cv.Invalid, match="not reserved"):
        ids.resolve(0, 17)
    assert not ids.assigned


def test_resolve_rejects_endpoint_reused_for_another_slot() -> None:
    ids = EndpointIds()
    ids.reserve(17)
    assert ids.resolve(0, 17) == 17
    with pytest.raises(cv.Invalid, match="already assigned"):
        ids.resolve(1, 17)
    assert ids.assigned == {0: 17}


def test_resolve_automatic_endpoint_can_be_read_again() -> None:
    ids = EndpointIds()
    ids.reserve(1)
    assert ids.resolve(0) == 2
    assert ids.resolve(0) == 2
    assert ids.resolve(0, 2) == 2
    assert ids.resolve(1, 1) == 1
