"""Unit tests for the sx126x and sx127x packet interface platforms."""

import pytest

from esphome import config_validation as cv
from esphome.components.packet_interface import CONF_MAX_PACKET_SIZE
from esphome.components.sx126x import packet_interface as sx126x_packet_interface
from esphome.components.sx127x import packet_interface as sx127x_packet_interface


@pytest.mark.parametrize(
    ("schema", "radio_key"),
    [
        (sx126x_packet_interface.CONFIG_SCHEMA, "sx126x_id"),
        (sx127x_packet_interface.CONFIG_SCHEMA, "sx127x_id"),
    ],
)
def test_max_packet_size_is_radio_limit(
    setup_core, schema: cv.Schema, radio_key: str
) -> None:
    """A radio packet holds at most 255 bytes, which is also the default."""
    assert schema({radio_key: "radio"})[CONF_MAX_PACKET_SIZE] == 255
    with pytest.raises(cv.Invalid):
        schema({radio_key: "radio", CONF_MAX_PACKET_SIZE: 256})
