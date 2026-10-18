from ipaddress import IPv4Address

import esphome.codegen as cg
from esphome.components import packet_interface
from esphome.components.packet_interface import (
    CONF_MAX_PACKET_SIZE,
    new_packet_interface,
    packet_interface_schema,
)
from esphome.components.packet_interface.layers import CONF_ADDRESSING
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE, HexInt
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import CONF_ADDRESSES, CONF_UDP_ID, DOMAIN as UDP_DOMAIN, UDP_SCHEMA, udp_ns

CODEOWNERS = ["@clydebarrow"]

DEPENDENCIES = ["udp"]

# Size of the receive buffer of the udp component
MAX_PACKET_SIZE = 508

UdpPacketInterface = udp_ns.class_(
    "UdpPacketInterface",
    packet_interface.PacketInterface,
)

CONFIG_SCHEMA = packet_interface_schema(
    UdpPacketInterface, MAX_PACKET_SIZE, MAX_PACKET_SIZE
).extend(UDP_SCHEMA)


def _peer(udp_config: ConfigType) -> IPv4Address | None:
    """The one peer, when the UDP component sends to a single unicast IPv4 address.

    An address ending in .255 is taken as a subnet broadcast, as the subnet is not known here.
    """
    match udp_config[CONF_ADDRESSES]:
        case [IPv4Address() as address] if not (
            address.is_multicast or address.packed[3] == 255
        ):
            return address
    return None


def _udp_config(full_config: ConfigType, config: ConfigType) -> ConfigType:
    return next(
        udp for udp in full_config[UDP_DOMAIN] if udp[CONF_ID] == config[CONF_UDP_ID]
    )


def _final_validate(config: ConfigType) -> ConfigType:
    if (
        CONF_ADDRESSING in config
        and (peer := _peer(_udp_config(fv.full_config.get(), config))) is not None
    ):
        raise cv.Invalid(
            f"The UDP component sends only to {peer}, so packets are already addressed; "
            f"'{CONF_ADDRESSING}' is only for broadcast or multicast",
            path=[CONF_ADDRESSING],
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    udp_var = await cg.get_variable(config[CONF_UDP_ID])
    cg.add(udp_var.set_should_listen())
    # The udp component sends to its addresses through its broadcast socket
    cg.add(udp_var.set_should_broadcast())
    peer = _peer(_udp_config(CORE.config, config))
    peer_bytes = peer.packed if peer is not None else bytes(4)
    # The copy buffer's size is known here, so it is a static array rather than allocated at run time
    tx_buffer = f"{config[CONF_ID].id}__tx_buffer"
    cg.add_global(
        cg.RawStatement(f"static uint8_t {tx_buffer}[{config[CONF_MAX_PACKET_SIZE]}];")
    )
    await new_packet_interface(
        config, udp_var, cg.RawExpression(tx_buffer), [HexInt(x) for x in peer_bytes]
    )
