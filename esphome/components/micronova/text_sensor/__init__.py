import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from .. import (
    CONF_MEMORY_ADDRESS,
    CONF_MEMORY_LOCATION,
    CONF_MICRONOVA_ID,
    MICRONOVA_LISTENER_SCHEMA,
    MicroNova,
    MicroNovaFunctions,
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
        ).extend(
            MICRONOVA_LISTENER_SCHEMA(
                default_memory_location=0x00, default_memory_address=0x21
            )
        ),
    }
)


async def to_code(config):
    mv = await cg.get_variable(config[CONF_MICRONOVA_ID])

    if stove_state_config := config.get(CONF_STOVE_STATE):
        sens = await text_sensor.new_text_sensor(stove_state_config, mv)
        cg.add(mv.register_micronova_listener(sens))
        cg.add(sens.set_memory_location(stove_state_config[CONF_MEMORY_LOCATION]))
        cg.add(sens.set_memory_address(stove_state_config[CONF_MEMORY_ADDRESS]))
        cg.add(sens.set_function(MicroNovaFunctions.STOVE_FUNCTION_STOVE_STATE))
