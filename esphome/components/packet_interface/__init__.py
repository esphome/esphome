from collections.abc import Callable
import importlib
import pkgutil
from typing import Any

import esphome.codegen as cg
from esphome.components.const import CONF_MAX_PACKET_SIZE
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE, ID
from esphome.cpp_generator import MockObj, MockObjClass
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import layers
from .layers import (
    ADDRESSING,
    BROADCAST_ADDRESS,
    CONF_ADDRESSING,
    CONF_DESTINATION,
    CONF_PEERS,
    STACK_ORDER,
    PacketInterface,
    build_stack,
    check_packet_sizes,
    check_session,
    check_single_peer,
    packet_interface_ns,
    stack_auto_load,
    stack_schema,
)

CODEOWNERS = ["@clydebarrow"]
DOMAIN = "packet_interface"
IS_PLATFORM_COMPONENT = True

CONF_PACKET_INTERFACE = "packet_interface"

# Each module in layers registers a layer type; import them all before building the schemas from the registry
for module_info in pkgutil.iter_modules(layers.__path__):
    importlib.import_module(f".layers.{module_info.name}", package=__package__)

LINK_LAYER_SCHEMAS = {
    cv.Optional(CONF_ADDRESSING): ADDRESSING.schema(),
    **stack_schema(),
}

__all__ = [
    "CONF_MAX_PACKET_SIZE",
    "CONF_PACKET_INTERFACE",
    "interface_layers",
    "interface_packet_size",
    "PacketInterface",
    "final_validate_interface",
    "new_packet_interface",
    "packet_interface_ns",
    "packet_interface_schema",
]


def AUTO_LOAD(config: list[ConfigType] | None = None) -> list[str]:
    """Load only what the configured layers use, on the link and in each addressing peer.

    Without a config, as tooling that lists dependencies calls it, everything a layer could load.
    """
    if config is None:
        return ["binary_sensor", "xxtea"]
    loads: set[str] = set()
    for interface in config:
        loads |= stack_auto_load(interface)
        for peer in interface.get(CONF_ADDRESSING, {}).get(CONF_PEERS, []):
            loads |= stack_auto_load(peer)
    return sorted(loads)


def _validate_layers(config: ConfigType) -> ConfigType:
    """With peers, the layers go in each peer; otherwise check them against the one destination."""
    check_session(config)
    size = config[CONF_MAX_PACKET_SIZE]
    if (addressing := config.get(CONF_ADDRESSING)) is None:
        check_packet_sizes(config, size)
        return config
    size = ADDRESSING.get_type(addressing).packet_size(addressing, size)
    if CONF_PEERS in addressing:
        for kind in STACK_ORDER:
            if kind.key in config:
                raise cv.Invalid(
                    f"With '{CONF_PEERS}', '{kind.key}' goes in each peer",
                    path=[kind.key],
                )
        for index, peer in enumerate(addressing[CONF_PEERS]):
            try:
                check_packet_sizes(peer, size)
            except cv.Invalid as err:
                err.prepend([CONF_ADDRESSING, CONF_PEERS, index])
                raise
        return config
    addressing.setdefault(CONF_DESTINATION, BROADCAST_ADDRESS)
    check_single_peer(config, addressing[CONF_DESTINATION])
    check_packet_sizes(config, size)
    return config


def _holds_id(config: ConfigType, interface_id: ID) -> bool:
    """Whether the id is that of this link or peer, or one in its layers, such as an addressing port's."""
    return config[CONF_ID].id == interface_id.id or any(
        isinstance(value, ID) and value.id == interface_id.id
        for key in (CONF_ADDRESSING, *(kind.key for kind in STACK_ORDER))
        if isinstance(layer := config.get(key), dict)
        for value in layer.values()
    )


def _find_interface(
    interface_id: ID, full_config: ConfigType
) -> tuple[ConfigType, ConfigType]:
    """The link holding the packet interface with this id, and the link or peer configuration holding its layers."""
    for interface in full_config.get(DOMAIN, []):
        if _holds_id(interface, interface_id):
            return interface, interface
        for peer in interface.get(CONF_ADDRESSING, {}).get(CONF_PEERS, []):
            if _holds_id(peer, interface_id):
                return interface, peer
    raise cv.Invalid(f"'{interface_id.id}' is not a packet interface")


def interface_layers(interface_id: ID) -> ConfigType:
    """During final validation, the configuration holding the layers of the packet interface with this id.

    The id is a link's, an addressing peer's, or a layer's: applications send through top(), so they use every
    layer of that link or peer.
    """
    return _find_interface(interface_id, fv.full_config.get())[1]


def interface_packet_size(interface_id: ID) -> int:
    """During code generation, the largest packet that top() of the packet interface with this id takes."""
    link, layers_config = _find_interface(interface_id, CORE.config)
    size = link[CONF_MAX_PACKET_SIZE]
    if (addressing := link.get(CONF_ADDRESSING)) is not None:
        size = ADDRESSING.get_type(addressing).packet_size(addressing, size)
    return check_packet_sizes(layers_config, size)


def final_validate_interface(key: str) -> Callable[[ConfigType], ConfigType]:
    """Final validation for an application: a link with peers must be used through one of the peers."""

    def validator(config: ConfigType) -> ConfigType:
        interface_id: ID = config[key]
        interface = interface_layers(interface_id)
        if peers := interface.get(CONF_ADDRESSING, {}).get(CONF_PEERS):
            raise cv.Invalid(
                f"'{interface_id.id}' has peers, so use the id of one of them, such as '{peers[0][CONF_ID].id}'",
                path=[key],
            )
        return config

    return validator


def packet_interface_schema(
    class_: MockObjClass,
    default_max_packet_size: int,
    max_packet_size_limit: int = 4096,
) -> cv.Schema:
    """Schema for a link platform, with the layers that can be stacked on it."""
    return (
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(class_),
                cv.Optional(
                    CONF_MAX_PACKET_SIZE, default=default_max_packet_size
                ): cv.int_range(16, max_packet_size_limit),
                **LINK_LAYER_SCHEMAS,
            }
        )
        .extend(cv.COMPONENT_SCHEMA)
        .add_extra(_validate_layers)
    )


async def new_packet_interface(config: ConfigType, *args: Any) -> MockObj:
    """Create the link layer and the layers configured on it, from the bottom up.

    The link's constructor takes its maximum packet size, usually as a std::span over a buffer that code generation
    sized from the configuration, so the two cannot differ.

    Returns the link layer; applications reach the top layer with PacketInterface::top(). With several addressing
    peers, applications use the peers' ids instead.
    """
    var = cg.new_Pvariable(config[CONF_ID], *args)
    cg.add(var.set_name(config[CONF_ID].id))
    await cg.register_component(var, config)
    size = config[CONF_MAX_PACKET_SIZE]
    if (addressing := config.get(CONF_ADDRESSING)) is None:
        await build_stack(config, var, size)
        return var
    for port, layers_config, port_size in await ADDRESSING.get_type(addressing).build(
        addressing, var, size
    ):
        await build_stack(
            config if layers_config is None else layers_config, port, port_size
        )
    return var
