"""The kinds of layer that can be stacked on a packet interface, and the registry of their implementations.

Each module in this package implements one layer type and registers it with its kind, for example:

    @RELIABILITY.register("hdlc", schema=HDLC_SCHEMA, single_peer=lambda config: True)
    async def build_hdlc(config: ConfigType, lower: MockObj) -> MockObj:
        ...

The packet_interface component imports every module here before it builds its schemas, so a new layer type needs
only a new module. The kinds, and the order in which they are stacked, are fixed here.
"""

from collections.abc import Awaitable, Callable
from dataclasses import dataclass
from typing import Any

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ENCRYPTION, CONF_FRAGMENTATION, CONF_KEY, CONF_TYPE
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

packet_interface_ns = cg.esphome_ns.namespace("packet_interface")
PacketInterface = packet_interface_ns.class_("PacketInterface", cg.Component)

CONF_ADDRESSING = "addressing"
CONF_DESTINATION = "destination"
CONF_PEERS = "peers"
CONF_RELIABILITY = "reliability"
CONF_SESSION = "session"

BROADCAST_ADDRESS = 0xFFFF

# Builds a layer above lower and returns it. An addressing layer instead returns a list of (port, config) pairs, one
# for each stack it carries, where config holds the layers for that stack, or is None for the layers on the link.
LayerBuilder = Callable[[ConfigType, MockObj], Awaitable[Any]]


def _never(config: ConfigType) -> bool:
    return False


def _nothing(config: ConfigType) -> list[str]:
    return []


def _same_size(config: ConfigType, size: int) -> int:
    return size


def _no_limit(config: ConfigType) -> int | None:
    return None


def _fits(config: ConfigType, size: int, capacity: int | None) -> None:
    """Accept any layer below."""


@dataclass(frozen=True)
class LayerType:
    """One implementation of a kind of layer."""

    # Returns the schema; called once every layer type is registered, as some schemas contain other layers
    schema_factory: Callable[[], Any]
    build: LayerBuilder
    # True if this configuration keeps state for one peer, so cannot be used with broadcast addressing
    single_peer: Callable[[ConfigType], bool]
    # True if this configuration works only when the peer sends packets back, which a session layer makes sure of
    needs_session: Callable[[ConfigType], bool]
    # The components this configuration needs loaded
    auto_load: Callable[[ConfigType], list[str]]
    # The largest packet this configuration takes, from the largest that the layer below takes, as the C++ works it out
    packet_size: Callable[[ConfigType, int], int]
    # How many packets this configuration takes at once, or None for no limit
    send_capacity: Callable[[ConfigType], int | None]
    # Raises cv.Invalid if this configuration cannot work above a layer with this packet size and send capacity
    check_lower: Callable[[ConfigType, int, int | None], None]


class LayerKind:
    """A kind of layer, such as encryption, with the types that implement it."""

    def __init__(
        self, key: str, default_type: str, shorthand: str | None = None
    ) -> None:
        self.key = key
        self.default_type = default_type
        # The option that a plain string value sets, as in `encryption: my key`
        self.shorthand = shorthand
        self.types: dict[str, LayerType] = {}
        # Set once the schema is built, as types registered after that would not be in it
        self.frozen = False

    def register(
        self,
        name: str,
        *,
        schema: Any = None,
        schema_factory: Callable[[], Any] | None = None,
        single_peer: Callable[[ConfigType], bool] = _never,
        needs_session: Callable[[ConfigType], bool] = _never,
        auto_load: Callable[[ConfigType], list[str]] = _nothing,
        packet_size: Callable[[ConfigType, int], int] = _same_size,
        send_capacity: Callable[[ConfigType], int | None] = _no_limit,
        check_lower: Callable[[ConfigType, int, int | None], None] = _fits,
    ) -> Callable[[LayerBuilder], LayerBuilder]:
        """Register a type with its schema, or with a function returning a schema that contains other layers."""

        def fixed_schema() -> Any:
            return schema

        def decorator(build: LayerBuilder) -> LayerBuilder:
            if self.frozen:
                raise ValueError(
                    f"{self.key} layer type '{name}' is registered after the schemas were built; register it "
                    "before the packet_interface component is imported"
                )
            if name in self.types:
                raise ValueError(
                    f"{self.key} layer type '{name}' is already registered"
                )
            self.types[name] = LayerType(
                schema_factory or fixed_schema,
                build,
                single_peer,
                needs_session,
                auto_load,
                packet_size,
                send_capacity,
                check_lower,
            )
            return build

        return decorator

    def _expand(self, value: Any) -> Any:
        """Allow a layer to be enabled with no options, as in `reliability:`, or with its shorthand."""
        if value is None:
            return {}
        if self.shorthand is not None and isinstance(value, str):
            return {self.shorthand: value}
        return value

    def schema(self) -> Callable[[Any], ConfigType]:
        """The schema for this kind; no type can be registered after this."""
        self.frozen = True
        return cv.All(
            self._expand,
            cv.typed_schema(
                {name: layer.schema_factory() for name, layer in self.types.items()},
                default_type=self.default_type,
            ),
        )

    def get_type(self, config: ConfigType) -> LayerType:
        return self.types[config[CONF_TYPE]]


# An addressing type's configuration has either a destination or a list of peers, each with an id, a destination
# and its own layers.
ADDRESSING = LayerKind(CONF_ADDRESSING, "basic")
ENCRYPTION = LayerKind(CONF_ENCRYPTION, "xxtea", shorthand=CONF_KEY)
RELIABILITY = LayerKind(CONF_RELIABILITY, "hdlc")
FRAGMENTATION = LayerKind(CONF_FRAGMENTATION, "basic")
SESSION = LayerKind(CONF_SESSION, "basic")
# The kinds stacked above the link, or above each port of an addressing layer, from the bottom up
STACK_ORDER = (ENCRYPTION, RELIABILITY, FRAGMENTATION, SESSION)


def stack_schema() -> dict:
    """Schema entries for the layers of one stack; call only once all layer types are registered."""
    return {cv.Optional(kind.key): kind.schema() for kind in STACK_ORDER}


def check_session(config: ConfigType) -> None:
    """Layers that need packets back from the peer need a session layer, whose keep-alives send them."""
    if CONF_SESSION in config:
        return
    for kind in STACK_ORDER:
        if (layer := config.get(kind.key)) is not None and kind.get_type(
            layer
        ).needs_session(layer):
            raise cv.Invalid(
                f"This '{kind.key}' needs packets from the peer, so it needs a '{CONF_SESSION}' layer, whose "
                "keep-alives make sure they are sent",
                path=[kind.key],
            )


def check_single_peer(config: ConfigType, destination: int) -> None:
    """Layers that keep state for one peer need a destination other than broadcast."""
    if destination != BROADCAST_ADDRESS:
        return
    for kind in STACK_ORDER:
        if (layer := config.get(kind.key)) is not None and kind.get_type(
            layer
        ).single_peer(layer):
            raise cv.Invalid(
                f"This '{kind.key}' works between two devices only, so it needs a '{CONF_DESTINATION}' other than "
                "broadcast",
                path=[kind.key],
            )


def remaining_packet_size(size: int, used: int) -> int:
    return max(size - used, 0)


def check_packet_sizes(config: ConfigType, size: int) -> None:
    """Check each layer in config against the layer below it; size is the largest packet that the stack sits on takes."""
    capacity = None
    for kind in STACK_ORDER:
        if (layer := config.get(kind.key)) is None:
            continue
        layer_type = kind.get_type(layer)
        try:
            layer_type.check_lower(layer, size, capacity)
        except cv.Invalid as err:
            err.prepend([kind.key])
            raise
        size = layer_type.packet_size(layer, size)
        capacity = layer_type.send_capacity(layer)


def stack_auto_load(config: ConfigType) -> set[str]:
    """The components that the layers configured in config need loaded."""
    loads: set[str] = set()
    for kind in STACK_ORDER:
        if (layer := config.get(kind.key)) is not None:
            loads.update(kind.get_type(layer).auto_load(layer))
    return loads


async def build_stack(config: ConfigType, lower: MockObj) -> None:
    """Create the layers configured in config, from the bottom up, above lower."""
    for kind in STACK_ORDER:
        if (layer := config.get(kind.key)) is not None:
            lower = await kind.get_type(layer).build(layer, lower)
