from collections.abc import Callable
import importlib
import pkgutil
from typing import Any

import esphome.codegen as cg
from esphome.components.const import CONF_MAX_PACKET_SIZE
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import ID
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
    CONF_SESSION,
    STACK_ORDER,
    PacketInterface,
    build_stack,
    check_packet_sizes,
    check_session,
    check_single_peer,
    layer_needing_session,
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


def interface_layers(interface_id: ID) -> ConfigType | None:
    """During final validation, the configuration holding the layers of the packet interface with this id.

    The id is a link's, or an addressing peer's.
    """
    for interface in fv.full_config.get().get(DOMAIN, []):
        if interface[CONF_ID].id == interface_id.id:
            return interface
        for peer in interface.get(CONF_ADDRESSING, {}).get(CONF_PEERS, []):
            if peer[CONF_ID].id == interface_id.id:
                return peer
    return None


def final_validate_interface(key: str) -> Callable[[ConfigType], ConfigType]:
    """Final validation for an application: a link with peers must be used through one of the peers."""

    def validator(config: ConfigType) -> ConfigType:
        interface_id: ID = config[key]
        interface = interface_layers(interface_id)
        if interface is not None and (
            peers := interface.get(CONF_ADDRESSING, {}).get(CONF_PEERS)
        ):
            raise cv.Invalid(
                f"'{interface_id.id}' has peers, so use the id of one of them, such as '{peers[0][CONF_ID].id}'",
                path=[key],
            )
        return config

    return validator


def _check_provided_session(config: ConfigType) -> ConfigType:
    """A link's own session does not send packets through the layers, so none can need them from the peer."""
    if key := layer_needing_session(config):
        raise cv.Invalid(
            f"This '{key}' needs packets from the peer, which this link does not send through its layers, "
            "so it cannot be used on this link",
            path=[key],
        )
    return config


def packet_interface_schema(
    class_: MockObjClass,
    default_max_packet_size: int,
    max_packet_size_limit: int = 4096,
    provides: tuple[str, ...] = (),
) -> cv.Schema:
    """Schema for a link platform; provides names the layer keys, such as reliability, that the link does itself."""
    link_layers = {
        key: (
            cv.invalid(f"This link provides '{key}' itself, so it cannot be configured")
            if key in provides
            else schema
        )
        for key, schema in LINK_LAYER_SCHEMAS.items()
    }
    schema = cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(class_),
            cv.Optional(
                CONF_MAX_PACKET_SIZE, default=default_max_packet_size
            ): cv.int_range(16, max_packet_size_limit),
            **link_layers,
        }
    ).extend(cv.COMPONENT_SCHEMA)
    if CONF_SESSION in provides:
        schema = schema.add_extra(_check_provided_session)
    return schema.add_extra(_validate_layers)


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
    if (addressing := config.get(CONF_ADDRESSING)) is None:
        await build_stack(config, var)
        return var
    for port, layers_config in await ADDRESSING.get_type(addressing).build(
        addressing, var
    ):
        await build_stack(config if layers_config is None else layers_config, port)
    return var
