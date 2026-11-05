"""Tests for the sx127x packet_transport parent validation."""

from typing import Any

import pytest

from esphome.components.sx127x.packet_transport import _validate_parent
import esphome.config_validation as cv
from esphome.core import Lambda


def _parent(**overrides: Any) -> dict[str, Any]:
    config = {"modulation": "FSK", "packet_mode": True, "payload_length": 8}
    config.update(overrides)
    return config


@pytest.mark.parametrize(
    "overrides",
    [{}, {"payload_length": 64}, {"modulation": "LORA", "payload_length": 200}],
)
def test_parent_with_short_packets_is_accepted(overrides: dict[str, Any]) -> None:
    """packet_transport works with packets that fit the 64 byte FIFO."""
    parent = _parent(**overrides)
    assert _validate_parent(parent) is parent


@pytest.mark.parametrize(
    "overrides",
    [{"payload_length": 65}, {"payload_length": Lambda("return 0;")}],
)
def test_parent_with_long_packets_is_rejected(overrides: dict[str, Any]) -> None:
    """Long packets are receive-only, so packet_transport is rejected."""
    with pytest.raises(cv.Invalid, match="not supported"):
        _validate_parent(_parent(**overrides))
