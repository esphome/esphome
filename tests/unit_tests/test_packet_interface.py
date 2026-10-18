"""Unit tests for the packet_interface component."""

from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components import packet_interface
from esphome.components.packet_interface import CONF_MAX_PACKET_SIZE
from esphome.components.packet_interface.layers import (
    CONF_RELIABILITY,
    CONF_SESSION,
    RELIABILITY,
    STACK_ORDER,
    LayerKind,
)
from esphome.components.packet_interface.layers.basic_session import CONF_KEEP_ALIVE
from esphome.components.packet_interface.layers.hdlc_reliability import (
    CONF_MAX_QUEUE_LENGTH,
)
from esphome.components.uart import packet_interface as uart_packet_interface
from esphome.components.udp import packet_interface as udp_packet_interface
from esphome.const import CONF_ENCRYPTION


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
        CONF_ENCRYPTION: {"xxtea"},
        CONF_RELIABILITY: {"hdlc"},
        CONF_SESSION: {"basic"},
    }


async def _build(config: dict, lower: Any) -> Any:
    return lower


def test_registered_type_selectable(setup_core) -> None:
    """A newly registered layer type can be selected with type."""
    kind = LayerKind("test_kind", "test")
    kind.register("test", schema=cv.Schema({}))(_build)
    config = kind.schema()({"type": "test"})
    assert config["type"] == "test"
    assert kind.get_type(config).build is _build


def test_duplicate_layer_type_rejected(setup_core) -> None:
    """Registering a type under a name already in use is an error, not a silent replacement."""
    kind = LayerKind("test_kind", "test")
    kind.register("test", schema=cv.Schema({}))(_build)
    with pytest.raises(ValueError, match="already registered"):
        kind.register("test", schema=cv.Schema({}))(_build)


def test_late_registration_rejected(setup_core) -> None:
    """A type registered after the schemas were built would be missing from them, so it is an error."""
    with pytest.raises(ValueError, match="after the schemas were built"):
        RELIABILITY.register("late", schema=cv.Schema({}))(_build)


def test_session_defaults(setup_core) -> None:
    """An empty session key selects the basic session, timing out after three keep-alives."""
    session = uart_packet_interface.CONFIG_SCHEMA({CONF_SESSION: None})[CONF_SESSION]
    assert session["type"] == "basic"
    assert session[CONF_KEEP_ALIVE].total_milliseconds == 10000
    assert session["timeout"].total_milliseconds == 30000


def test_session_timeout(setup_core) -> None:
    """The timeout follows keep_alive unless given, and must be longer than it."""
    session = uart_packet_interface.CONFIG_SCHEMA(
        {CONF_SESSION: {CONF_KEEP_ALIVE: "2s"}}
    )[CONF_SESSION]
    assert session["timeout"].total_milliseconds == 6000
    with pytest.raises(cv.Invalid):
        uart_packet_interface.CONFIG_SCHEMA(
            {CONF_SESSION: {CONF_KEEP_ALIVE: "5s", "timeout": "5s"}}
        )
    with pytest.raises(cv.Invalid):
        uart_packet_interface.CONFIG_SCHEMA({CONF_SESSION: {CONF_KEEP_ALIVE: "500ms"}})


def test_encryption_key_only(setup_core) -> None:
    """The key can be given on its own, and selects XXTEA."""
    encryption = uart_packet_interface.CONFIG_SCHEMA({CONF_ENCRYPTION: "secret"})[
        CONF_ENCRYPTION
    ]
    assert encryption["type"] == "xxtea"
    assert encryption["key"] == "secret"
    assert encryption["rolling_code_enable"] is False


def test_encryption_requires_key(setup_core) -> None:
    """An encryption layer without a key is rejected."""
    with pytest.raises(cv.Invalid):
        uart_packet_interface.CONFIG_SCHEMA({CONF_ENCRYPTION: {}})
    with pytest.raises(cv.Invalid):
        uart_packet_interface.CONFIG_SCHEMA({CONF_ENCRYPTION: ""})


@pytest.mark.parametrize(
    ("config", "loads"),
    [
        ({}, []),
        ({CONF_RELIABILITY: None, CONF_SESSION: None}, []),
        ({CONF_SESSION: {"binary_sensor": {"name": "Link"}}}, ["binary_sensor"]),
        ({CONF_ENCRYPTION: "secret"}, ["xxtea"]),
    ],
)
def test_auto_load_follows_layers(
    setup_core, config: dict[str, Any], loads: list[str]
) -> None:
    """Only the components that the configured layers use are loaded."""
    validated = uart_packet_interface.CONFIG_SCHEMA(config)
    assert packet_interface.AUTO_LOAD([validated]) == loads


def test_auto_load_without_config() -> None:
    """Tooling that lists dependencies gets everything a layer could load."""
    assert packet_interface.AUTO_LOAD() == ["binary_sensor", "xxtea"]
