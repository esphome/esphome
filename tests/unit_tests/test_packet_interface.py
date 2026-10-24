"""Unit tests for the packet_interface component."""

import pytest

from esphome import config_validation as cv
from esphome.components import packet_interface
from esphome.components.packet_interface import CONF_MAX_PACKET_SIZE
from esphome.components.uart import packet_interface as uart_packet_interface
from esphome.components.udp import packet_interface as udp_packet_interface


@pytest.mark.parametrize(
    ("schema", "default", "limit"),
    [
        (uart_packet_interface.CONFIG_SCHEMA, 508, 4096),
        (udp_packet_interface.CONFIG_SCHEMA, 508, 508),
    ],
)
def test_platform_max_packet_size(
    setup_core, schema: cv.Schema, default: int, limit: int
) -> None:
    """Each platform has its own default and upper limit for max_packet_size."""
    assert schema({})[CONF_MAX_PACKET_SIZE] == default
    assert schema({CONF_MAX_PACKET_SIZE: limit})[CONF_MAX_PACKET_SIZE] == limit
    with pytest.raises(cv.Invalid):
        schema({CONF_MAX_PACKET_SIZE: limit + 1})
    with pytest.raises(cv.Invalid):
        schema({CONF_MAX_PACKET_SIZE: 15})


def test_schema_default(setup_core) -> None:
    """packet_interface_schema applies the default it is given."""
    schema = packet_interface.packet_interface_schema(
        packet_interface.PacketInterface, 100
    )
    assert schema({})[CONF_MAX_PACKET_SIZE] == 100
