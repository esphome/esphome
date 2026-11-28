import esphome.codegen as cg
from esphome.components.packet_transport import (
    CONF_PROVIDERS,
    PacketTransport,
    get_sensors,
    new_packet_transport,
    transport_schema,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_types import PollingComponent
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import (
    CONF_PACKET_MODE,
    CONF_PAYLOAD_LENGTH,
    CONF_RX_START,
    CONF_SX1231_ID,
    CONF_SYNC_VALUE,
    SX1231,
    SX1231Listener,
    sx1231_ns,
)

SX1231Transport = sx1231_ns.class_(
    "SX1231Transport", PacketTransport, PollingComponent, SX1231Listener
)

CONFIG_SCHEMA = transport_schema(SX1231Transport).extend(
    {
        cv.GenerateID(CONF_SX1231_ID): cv.use_id(SX1231),
    }
)


def _final_validate(config: ConfigType) -> ConfigType:
    radio = fv.get_declaring_config(config[CONF_SX1231_ID])
    if not radio[CONF_PACKET_MODE]:
        raise cv.Invalid(
            f"The radio's '{CONF_PACKET_MODE}' must be true, as the transport sends packets",
            path=[CONF_SX1231_ID],
        )
    if radio[CONF_PAYLOAD_LENGTH] != 0:
        raise cv.Invalid(
            f"Packets vary in length, so the radio's '{CONF_PAYLOAD_LENGTH}' must be 0",
            path=[CONF_SX1231_ID],
        )
    if not radio[CONF_SYNC_VALUE]:
        raise cv.Invalid(
            f"The radio needs a '{CONF_SYNC_VALUE}', so that it does not take noise for packets",
            path=[CONF_SX1231_ID],
        )
    # Providers are listed, or only named by the sensors that take their values
    receives = config[CONF_PROVIDERS] or any(get_sensors(config[CONF_ID]))
    if receives and not radio[CONF_RX_START]:
        raise cv.Invalid(
            f"The transport needs packets from its providers, but the radio sleeps unless its '{CONF_RX_START}' "
            "is true",
            path=[CONF_SX1231_ID],
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var, _ = await new_packet_transport(config)
    await cg.register_parented(var, config[CONF_SX1231_ID])
