import esphome.codegen as cg
from esphome.components import packet_interface
from esphome.components.packet_interface import (
    CONF_MAX_PACKET_SIZE,
    new_packet_interface,
    packet_interface_schema,
)
from esphome.components.packet_interface.radio_airtime import max_busy_ms
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import (
    CONF_MODULATION,
    CONF_PAYLOAD_LENGTH,
    CONF_SX127X_ID,
    DOMAIN,
    SX127x,
    SX127xListener,
    sx127x_ns,
)

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["sx127x"]

# The radio's payload length register is 8 bits
MAX_PACKET_SIZE = 255

# The FIFO limit outside LoRa mode
FSK_MAX_PACKET_SIZE = 64

SX127xPacketInterface = sx127x_ns.class_(
    "SX127xPacketInterface", packet_interface.PacketInterface, SX127xListener
)

CONFIG_SCHEMA = packet_interface_schema(
    SX127xPacketInterface, MAX_PACKET_SIZE, MAX_PACKET_SIZE
).extend({cv.GenerateID(CONF_SX127X_ID): cv.use_id(SX127x)})


def _final_validate(config: ConfigType) -> ConfigType:
    full_config = fv.full_config.get()
    radio = full_config.get_config_for_path(
        full_config.get_path_for_id(config[CONF_SX127X_ID])[:-1]
    )
    if radio[CONF_PAYLOAD_LENGTH] != 0:
        raise cv.Invalid(
            f"Packets vary in length, so the radio's '{CONF_PAYLOAD_LENGTH}' must be 0",
            path=[CONF_SX127X_ID],
        )
    if (
        radio[CONF_MODULATION] != "LORA"
        and config[CONF_MAX_PACKET_SIZE] > FSK_MAX_PACKET_SIZE
    ):
        raise cv.Invalid(
            f"Without LoRa modulation the radio sends at most {FSK_MAX_PACKET_SIZE} bytes",
            path=[CONF_MAX_PACKET_SIZE],
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    radio = await cg.get_variable(config[CONF_SX127X_ID])
    var = await new_packet_interface(config, radio)
    radio_config = next(
        conf for conf in CORE.config[DOMAIN] if conf[CONF_ID] == config[CONF_SX127X_ID]
    )
    cg.add(var.set_max_busy(max_busy_ms(radio_config, config[CONF_MAX_PACKET_SIZE])))
