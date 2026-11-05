import esphome.codegen as cg
from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)
import esphome.config_validation as cv
from esphome.cpp_types import PollingComponent
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import (
    CONF_PAYLOAD_LENGTH,
    CONF_SX127X_ID,
    SX127x,
    SX127xListener,
    is_long_packet,
    sx127x_ns,
)

SX127xTransport = sx127x_ns.class_(
    "SX127xTransport", PacketTransport, PollingComponent, SX127xListener
)

CONFIG_SCHEMA = transport_schema(SX127xTransport).extend(
    {
        cv.GenerateID(CONF_SX127X_ID): cv.use_id(SX127x),
    }
)


def _validate_parent(parent_config: ConfigType) -> ConfigType:
    if is_long_packet(parent_config):
        raise cv.Invalid(
            f"packet_transport cannot send with {CONF_PAYLOAD_LENGTH} > 64 or a lambda,"
            " that configuration is receive-only"
        )
    return parent_config


FINAL_VALIDATE_SCHEMA = cv.Schema(
    {cv.Required(CONF_SX127X_ID): fv.id_declaration_match_schema(_validate_parent)},
    extra=cv.ALLOW_EXTRA,
)


async def to_code(config: ConfigType) -> None:
    var, _ = await new_packet_transport(config)
    await cg.register_parented(var, config[CONF_SX127X_ID])
