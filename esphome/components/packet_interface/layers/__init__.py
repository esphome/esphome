"""The kinds of layer that can be stacked on a packet interface, and the registry of their implementations.

Each module in this package implements one layer type and registers it with its kind, for example:

    @RELIABILITY.register("hdlc", schema=HDLC_SCHEMA)
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
from esphome.const import CONF_ENCRYPTION, CONF_KEY, CONF_TYPE
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

packet_interface_ns = cg.esphome_ns.namespace("packet_interface")
PacketInterface = packet_interface_ns.class_("PacketInterface", cg.Component)

CONF_RELIABILITY = "reliability"
CONF_SESSION = "session"

# Builds a layer above lower and returns it
LayerBuilder = Callable[[ConfigType, MockObj], Awaitable[Any]]


def _nothing(config: ConfigType) -> list[str]:
    return []


@dataclass(frozen=True)
class LayerType:
    """One implementation of a kind of layer."""

    schema: Any
    build: LayerBuilder
    # The components this configuration needs loaded
    auto_load: Callable[[ConfigType], list[str]]


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
        schema: Any,
        auto_load: Callable[[ConfigType], list[str]] = _nothing,
    ) -> Callable[[LayerBuilder], LayerBuilder]:
        """Register a type with its schema."""

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
            self.types[name] = LayerType(schema, build, auto_load)
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
                {name: layer.schema for name, layer in self.types.items()},
                default_type=self.default_type,
            ),
        )

    def get_type(self, config: ConfigType) -> LayerType:
        return self.types[config[CONF_TYPE]]


ENCRYPTION = LayerKind(CONF_ENCRYPTION, "xxtea", shorthand=CONF_KEY)
RELIABILITY = LayerKind(CONF_RELIABILITY, "hdlc")
SESSION = LayerKind(CONF_SESSION, "basic")
# The kinds stacked above the link, from the bottom up
STACK_ORDER = (ENCRYPTION, RELIABILITY, SESSION)


def stack_schema() -> dict:
    """Schema entries for the layers of one stack; call only once all layer types are registered."""
    return {cv.Optional(kind.key): kind.schema() for kind in STACK_ORDER}


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
