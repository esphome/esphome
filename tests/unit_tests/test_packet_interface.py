"""Unit tests for the packet_interface component and its platforms."""

import hashlib
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components import packet_interface
from esphome.components.packet_interface import CONF_MAX_PACKET_SIZE
from esphome.components.packet_interface.layers import (
    CONF_ADDRESSING,
    CONF_FRAGMENTATION,
    CONF_RELIABILITY,
    CONF_SESSION,
    RELIABILITY,
    STACK_ORDER,
    LayerKind,
    xxtea_encryption,
)
from esphome.components.packet_interface.layers.basic_session import CONF_KEEP_ALIVE
from esphome.components.packet_interface.layers.hdlc_reliability import (
    CONF_MAX_QUEUE_LENGTH,
)
from esphome.components.uart import packet_interface as uart_packet_interface
from esphome.components.udp import packet_interface as udp_packet_interface
from esphome.const import CONF_ENCRYPTION
from esphome.core import ID
import esphome.final_validate as fv


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
    with pytest.raises(cv.Invalid):
        uart_packet_interface.CONFIG_SCHEMA({CONF_SESSION: {CONF_KEEP_ALIVE: "2h"}})
    with pytest.raises(cv.Invalid):
        uart_packet_interface.CONFIG_SCHEMA({CONF_SESSION: {"timeout": "4h"}})


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


def test_rolling_code_requires_session(setup_core) -> None:
    """The rolling code needs the session's keep-alives, so the peer sends this device's boot number back."""
    rolling = {CONF_ENCRYPTION: {"key": "secret", "rolling_code_enable": True}}
    with pytest.raises(cv.Invalid, match="session"):
        uart_packet_interface.CONFIG_SCHEMA(rolling)
    uart_packet_interface.CONFIG_SCHEMA({**rolling, CONF_SESSION: None})
    uart_packet_interface.CONFIG_SCHEMA({CONF_ENCRYPTION: "secret"})


def test_addressing_defaults_to_broadcast(setup_core) -> None:
    """Without a destination, packets are broadcast; broadcast can also be named."""
    for addressing in ({"address": 1}, {"address": 1, "destination": "broadcast"}):
        config = uart_packet_interface.CONFIG_SCHEMA({CONF_ADDRESSING: addressing})
        assert config[CONF_ADDRESSING]["destination"] == 0xFFFF
    with pytest.raises(cv.Invalid):
        uart_packet_interface.CONFIG_SCHEMA({CONF_ADDRESSING: {"address": 0xFFFF}})


@pytest.mark.parametrize(
    "layers",
    [
        {CONF_RELIABILITY: None},
        {CONF_SESSION: None},
        {
            CONF_ENCRYPTION: {"key": "secret", "rolling_code_enable": True},
            CONF_SESSION: None,
        },
    ],
)
def test_two_device_layers_need_destination(setup_core, layers: dict) -> None:
    """Layers that work between two devices only cannot be used with broadcast addressing."""
    with pytest.raises(cv.Invalid, match="destination"):
        uart_packet_interface.CONFIG_SCHEMA({CONF_ADDRESSING: {"address": 1}, **layers})
    uart_packet_interface.CONFIG_SCHEMA(
        {CONF_ADDRESSING: {"address": 1, "destination": 2}, **layers}
    )


def _peer(peer_id: str, destination: Any, **layers: Any) -> dict:
    return {"id": peer_id, "destination": destination, **layers}


def test_addressing_peers(setup_core) -> None:
    """Each peer has its own destination and layers."""
    config = uart_packet_interface.CONFIG_SCHEMA(
        {
            CONF_ADDRESSING: {
                "address": 1,
                "peers": [
                    _peer("peer_a", 2, reliability=None, session=None),
                    _peer("peer_b", 3, encryption="secret"),
                    _peer("everyone", "broadcast"),
                ],
            }
        }
    )
    peers = config[CONF_ADDRESSING]["peers"]
    assert [peer["destination"] for peer in peers] == [2, 3, 0xFFFF]
    assert CONF_RELIABILITY in peers[0]


@pytest.mark.parametrize(
    ("addressing", "layers", "match"),
    [
        ({"peers": [_peer("a", 2), _peer("b", 2)]}, {}, "different destination"),
        ({"peers": [_peer("a", "broadcast", reliability=None)]}, {}, "broadcast"),
        ({"peers": [_peer("a", 2)]}, {CONF_RELIABILITY: None}, "goes in each peer"),
        ({"destination": 2, "peers": [_peer("a", 3)]}, {}, "destination"),
    ],
)
def test_addressing_peers_invalid(
    setup_core, addressing: dict, layers: dict, match: str
) -> None:
    """Peers need different destinations, two-device layers need a destination, and layers go in the peers."""
    with pytest.raises(cv.Invalid, match=match):
        uart_packet_interface.CONFIG_SCHEMA(
            {CONF_ADDRESSING: {"address": 1, **addressing}, **layers}
        )


def test_layer_types_registered() -> None:
    """Importing packet_interface registers the built-in layer types."""
    assert {kind.key: set(kind.types) for kind in STACK_ORDER} == {
        CONF_ENCRYPTION: {"xxtea"},
        CONF_RELIABILITY: {"hdlc"},
        CONF_FRAGMENTATION: {"basic"},
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


def test_fragmentation_defaults(setup_core) -> None:
    """An empty fragmentation key joins packets of up to 1024 bytes."""
    fragmentation = uart_packet_interface.CONFIG_SCHEMA({CONF_FRAGMENTATION: None})[
        CONF_FRAGMENTATION
    ]
    assert fragmentation["type"] == "basic"
    assert fragmentation["max_packet_size"] == 1024
    with pytest.raises(cv.Invalid):
        uart_packet_interface.CONFIG_SCHEMA(
            {CONF_FRAGMENTATION: {"max_packet_size": 8}}
        )


@pytest.mark.parametrize(
    ("config", "loads"),
    [
        ({}, []),
        ({CONF_RELIABILITY: None, "session": None}, []),
        ({"session": {"binary_sensor": {"name": "Link"}}}, ["binary_sensor"]),
        ({CONF_ENCRYPTION: "secret"}, ["xxtea"]),
        (
            {
                CONF_ADDRESSING: {
                    "address": 1,
                    "peers": [_peer("a", 2, encryption="secret"), _peer("b", 3)],
                }
            },
            ["xxtea"],
        ),
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


def test_xxtea_key_stretched() -> None:
    """The key comes from PBKDF2 of the key text, not a single hash a captured packet could check guesses against."""
    key = xxtea_encryption.derive_key("Johnny Cash")
    digest = hashlib.pbkdf2_hmac(
        "sha256",
        b"Johnny Cash",
        b"esphome packet_interface xxtea",
        600_000,
    )
    assert key == [int.from_bytes(digest[i : i + 4], "little") for i in range(0, 32, 4)]
    assert len(key) == 8 and all(0 <= word < 2**32 for word in key)
    assert key != xxtea_encryption.derive_key("Johnny Cash ")
    sha = hashlib.sha256(b"Johnny Cash").digest()
    assert key[0] != int.from_bytes(sha[:4], "little")


@pytest.mark.parametrize(
    "addressing",
    [
        {"address": 1, "destination": 1},
        {"address": 1, "peers": [_peer("a", 2), _peer("b", 1)]},
    ],
)
def test_addressing_own_address_rejected(setup_core, addressing: dict) -> None:
    """A device cannot send to its own address, as its own packets are dropped as echoes."""
    with pytest.raises(cv.Invalid, match="own address"):
        uart_packet_interface.CONFIG_SCHEMA({CONF_ADDRESSING: addressing})


def test_application_needs_peer_id_with_several_peers(setup_core) -> None:
    """An application must use one peer's id, not the id of a link with several peers."""
    validator = packet_interface.final_validate_interface("packet_interface")
    token = fv.full_config.set(
        {
            "packet_interface": [
                {"id": ID("bus"), CONF_ADDRESSING: {"peers": [_peer("a", 2)]}},
                {"id": ID("single"), CONF_ADDRESSING: {"destination": 2}},
            ]
        }
    )
    try:
        with pytest.raises(cv.Invalid, match="several peers"):
            validator({"packet_interface": ID("bus")})
        assert validator({"packet_interface": ID("single")})
        assert validator({"packet_interface": ID("a")})
    finally:
        fv.full_config.reset(token)
