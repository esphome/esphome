import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ICON_POWER

from .. import (
    CONF_MICRONOVA_ID,
    MICRONOVA_ADDRESS_SCHEMA,
    MicroNova,
    MicroNovaListener,
    micronova_ns,
    to_code_micronova_listener,
)

CONF_STOVE = "stove"
CONF_MEMORY_DATA_ON = "memory_data_on"
CONF_MEMORY_DATA_OFF = "memory_data_off"

MicroNovaSwitch = micronova_ns.class_(
    "MicroNovaSwitch", switch.Switch, MicroNovaListener
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MICRONOVA_ID): cv.use_id(MicroNova),
        cv.Optional(CONF_STOVE): switch.switch_schema(
            MicroNovaSwitch,
            icon=ICON_POWER,
        )
        .extend(
            MICRONOVA_ADDRESS_SCHEMA(
                default_memory_location=0x00,
                default_memory_address=0x21,
                is_polling_component=True,
            )
        )
        .extend(
            {
                cv.Optional(CONF_MEMORY_DATA_OFF, default=0x06): cv.hex_int_range(),
                cv.Optional(CONF_MEMORY_DATA_ON, default=0x01): cv.hex_int_range(),
            }
        ),
    }
)


async def to_code(config):
    mv = await cg.get_variable(config[CONF_MICRONOVA_ID])

    if stove_config := config.get(CONF_STOVE):
        sw = await switch.new_switch(stove_config, mv)
        await to_code_micronova_listener(mv, sw, stove_config)
        cg.add(sw.set_memory_data_on(stove_config[CONF_MEMORY_DATA_ON]))
        cg.add(sw.set_memory_data_off(stove_config[CONF_MEMORY_DATA_OFF]))
