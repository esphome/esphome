import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from .. import (
    MicroNova,
    MicroNovaFunctions,
    CONF_MICRONOVA_ID,
    CONF_MEMORY_LOCATION,
    CONF_MEMORY_ADDRESS,
    MICRONOVA_LISTENER_SCHEMA,
    micronova_ns,
)

CONF_STOVE = "stove"
CONF_MEMORY_DATA_ON = "memory_data_on"
CONF_MEMORY_DATA_OFF = "memory_data_off"

ICON_STATE = "mdi:checkbox-marked-circle-outline"

TYPES = [
    CONF_STOVE,
]

MicroNovaSwitch = micronova_ns.class_("MicroNovaSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MICRONOVA_ID): cv.use_id(MicroNova),
        cv.Optional(CONF_STOVE): switch.switch_schema(
            MicroNovaSwitch,
            icon=ICON_STATE,
        )
        .extend(MICRONOVA_LISTENER_SCHEMA)
        .extend({cv.Optional(CONF_MEMORY_DATA_OFF, default=0x06): cv.hex_int_range()})
        .extend({cv.Optional(CONF_MEMORY_DATA_ON, default=0x01): cv.hex_int_range()}),
    }
)


async def to_code(config):
    mv = await cg.get_variable(config[CONF_MICRONOVA_ID])

    if stove_config := config.get(CONF_STOVE):
        sw = await switch.new_switch(stove_config, mv)
        cg.add(mv.set_stove(sw))
        cg.add(sw.set_memory_location(stove_config.get(CONF_MEMORY_LOCATION, 0x80)))
        cg.add(sw.set_memory_address(stove_config.get(CONF_MEMORY_ADDRESS, 0x21)))
        cg.add(sw.set_memory_data_on(stove_config[CONF_MEMORY_DATA_ON]))
        cg.add(sw.set_memory_data_off(stove_config[CONF_MEMORY_DATA_OFF]))
        cg.add(sw.set_function(MicroNovaFunctions.STOVE_FUNCTION_SWITCH))
