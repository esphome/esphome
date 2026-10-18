"""Unit tests for the packet_interface component."""

from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components import packet_interface
from esphome.components.packet_interface import CONF_MAX_PACKET_SIZE
from esphome.components.packet_interface.layers import (
    CONF_RELIABILITY,
    RELIABILITY,
    STACK_ORDER,
)
from esphome.components.packet_interface.layers.hdlc_reliability import (
    CONF_MAX_QUEUE_LENGTH,
)
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


def test_reliability_defaults(setup_core) -> None:
    """An empty reliability key selects the HDLC layer with its defaults."""
    config = uart_packet_interface.CONFIG_SCHEMA({CONF_RELIABILITY: None})
    reliability = config[CONF_RELIABILITY]
    assert reliability["type"] == "hdlc"
    assert reliability["timeout"].total_milliseconds == 200
    assert reliability[CONF_MAX_QUEUE_LENGTH] == 4
    assert reliability["max_retries"] == 5


def test_reliability_queue_length_limit(setup_core) -> None:
    """The queue cannot hold more frames than 3 bit sequence numbers allow."""
    with pytest.raises(cv.Invalid):
        uart_packet_interface.CONFIG_SCHEMA(
            {CONF_RELIABILITY: {CONF_MAX_QUEUE_LENGTH: 8}}
        )


def test_reliability_timeout_minimum(setup_core) -> None:
    """A timeout shorter than a few loop passes cannot work."""
    uart_packet_interface.CONFIG_SCHEMA({CONF_RELIABILITY: {"timeout": "50ms"}})
    with pytest.raises(cv.Invalid):
        uart_packet_interface.CONFIG_SCHEMA({CONF_RELIABILITY: {"timeout": "40ms"}})


def test_ack_delay_shorter_than_timeout(setup_core) -> None:
    """The acknowledgement must not wait longer than the peer's timeout."""
    reliability = uart_packet_interface.CONFIG_SCHEMA(
        {CONF_RELIABILITY: {"timeout": "500ms", "ack_delay": "100ms"}}
    )[CONF_RELIABILITY]
    assert reliability["ack_delay"].total_milliseconds == 100
    with pytest.raises(cv.Invalid, match="shorter"):
        uart_packet_interface.CONFIG_SCHEMA(
            {CONF_RELIABILITY: {"timeout": "500ms", "ack_delay": "500ms"}}
        )


def test_layer_types_registered() -> None:
    """Importing packet_interface registers the built-in layer types."""
    assert {kind.key: set(kind.types) for kind in STACK_ORDER} == {
        CONF_RELIABILITY: {"hdlc"},
    }


def test_registered_type_selectable(setup_core) -> None:
    """A newly registered layer type can be selected with type."""

    async def build(config: dict, lower: Any) -> Any:
        return lower

    RELIABILITY.register("test", schema=cv.Schema({}))(build)
    try:
        config = RELIABILITY.schema()({"type": "test"})
        assert config["type"] == "test"
        assert RELIABILITY.get_type(config).build is build
    finally:
        del RELIABILITY.types["test"]
