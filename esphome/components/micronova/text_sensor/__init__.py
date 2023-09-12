import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from .. import (
    MicroNova,
    MicroNovaFunctions,
    CONF_MICRONOVA_ID,
    CONF_MEMORY_LOCATION,
    CONF_MEMORY_ADDRESS,
    MICRONOVA_LISTENER_SCHEMA,
    micronova_ns,
)

CONF_STOVE_STATE = "stove_state"

MicroNovaTextSensor = micronova_ns.class_(
    "MicroNovaTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MICRONOVA_ID): cv.use_id(MicroNova),
        cv.Optional(CONF_STOVE_STATE): text_sensor.text_sensor_schema(
            MicroNovaTextSensor
        ).extend(MICRONOVA_LISTENER_SCHEMA),
    }
)


async def to_code(config):
    mv = await cg.get_variable(config[CONF_MICRONOVA_ID])

    if stove_state_config := config.get(CONF_STOVE_STATE):
        sens = await text_sensor.new_text_sensor(stove_state_config, mv)
        cg.add(mv.register_micronova_listener(sens))
        cg.add(
            sens.set_memory_location(stove_state_config.get(CONF_MEMORY_LOCATION, 0x00))
        )
        cg.add(
            sens.set_memory_address(stove_state_config.get(CONF_MEMORY_ADDRESS, 0x21))
        )
        cg.add(sens.set_function(MicroNovaFunctions.STOVE_FUNCTION_STOVE_STATE))
