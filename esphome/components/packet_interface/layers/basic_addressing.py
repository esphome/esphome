"""Addressing layer with 16-bit addresses, carrying one stack of layers or one for each of several peers."""

from typing import Any

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID
from esphome.core import ID
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from . import (
    ADDRESSING,
    BROADCAST_ADDRESS,
    CONF_DESTINATION,
    CONF_PEERS,
    PacketInterface,
    check_session,
    check_single_peer,
    packet_interface_ns,
    stack_schema,
)

CONF_PORT_ID = "port_id"

BasicAddressing = packet_interface_ns.class_("BasicAddressing", cg.Component)
AddressingPort = packet_interface_ns.class_("AddressingPort", PacketInterface)

_request_port_slot = cg.slot_counter("PACKET_INTERFACE_ADDRESSING_PORT_COUNT")


def _destination(value: Any) -> int:
    """A device address, or broadcast."""
    if isinstance(value, str) and value.lower() == "broadcast":
        return BROADCAST_ADDRESS
    return cv.hex_int_range(1, BROADCAST_ADDRESS)(value)


def _validate_peer(config: ConfigType) -> ConfigType:
    check_session(config)
    check_single_peer(config, config[CONF_DESTINATION])
    return config


def _unique_destinations(peers: list[ConfigType]) -> list[ConfigType]:
    destinations = [peer[CONF_DESTINATION] for peer in peers]
    if len(set(destinations)) != len(destinations):
        raise cv.Invalid("Each peer must have a different destination")
    return peers


def _not_own_address(config: ConfigType) -> ConfigType:
    """Packets from this device's own address are dropped as echoes, so it cannot be a destination."""
    address = config[CONF_ADDRESS]
    if config.get(CONF_DESTINATION) == address:
        raise cv.Invalid(
            "Must not be this device's own address", path=[CONF_DESTINATION]
        )
    for index, peer in enumerate(config.get(CONF_PEERS, [])):
        if peer[CONF_DESTINATION] == address:
            raise cv.Invalid(
                "Must not be this device's own address",
                path=[CONF_PEERS, index, CONF_DESTINATION],
            )
    return config


# A function, as each peer holds the other layers, whose schemas exist only once every layer type is registered
def _basic_addressing_schema() -> cv.Schema:
    peer_schema = cv.All(
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.declare_id(AddressingPort),
                cv.Required(CONF_DESTINATION): _destination,
                **stack_schema(),
            }
        ),
        _validate_peer,
    )
    return cv.All(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(BasicAddressing),
                cv.GenerateID(CONF_PORT_ID): cv.declare_id(AddressingPort),
                cv.Required(CONF_ADDRESS): cv.hex_int_range(1, BROADCAST_ADDRESS - 1),
                cv.Optional(CONF_DESTINATION): _destination,
                cv.Optional(CONF_PEERS): cv.All(
                    cv.ensure_list(peer_schema), cv.Length(min=1), _unique_destinations
                ),
            }
        ).extend(cv.COMPONENT_SCHEMA),
        cv.has_at_most_one_key(CONF_DESTINATION, CONF_PEERS),
        _not_own_address,
    )


async def _new_port(
    port_id: ID, addressing: MockObj, destination: int, only_port: bool
) -> MockObj:
    _request_port_slot(str(addressing))
    port = cg.new_Pvariable(port_id, addressing, destination, only_port)
    await cg.register_component(port, {})
    return port


@ADDRESSING.register("basic", schema_factory=_basic_addressing_schema)
async def build_basic_addressing(
    config: ConfigType, link: MockObj
) -> list[tuple[MockObj, ConfigType | None]]:
    cg.add_define("USE_PACKET_INTERFACE_BASIC_ADDRESSING")
    hub = cg.new_Pvariable(config[CONF_ID], link, config[CONF_ADDRESS])
    await cg.register_component(hub, config)
    if (peers := config.get(CONF_PEERS)) is None:
        port = await _new_port(
            config[CONF_PORT_ID], hub, config[CONF_DESTINATION], True
        )
        return [(port, None)]
    stacks = []
    # Each peer has its own stack, named after the peer for logs and saved preferences
    for peer in peers:
        port = await _new_port(peer[CONF_ID], hub, peer[CONF_DESTINATION], False)
        cg.add(port.set_name(peer[CONF_ID].id))
        stacks.append((port, peer))
    return stacks
