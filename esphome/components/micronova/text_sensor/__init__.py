import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from .. import (
    CONF_MICRONOVA_ID,
    MICRONOVA_ADDRESS_SCHEMA,
    MicroNova,
    MicroNovaListener,
    final_validate_address,
    micronova_ns,
    to_code_micronova_listener,
)
from ..const import CONF_STOVE_STATE

MicroNovaTextSensor = micronova_ns.class_(
    "MicroNovaTextSensor", text_sensor.TextSensor, MicroNovaListener
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MICRONOVA_ID): cv.use_id(MicroNova),
        cv.Optional(CONF_STOVE_STATE): text_sensor.text_sensor_schema(
            MicroNovaTextSensor
        ).extend(MICRONOVA_ADDRESS_SCHEMA(is_polling_component=True)),
    }
)


FINAL_VALIDATE_SCHEMA = final_validate_address


async def to_code(config):
    mv = await cg.get_variable(config[CONF_MICRONOVA_ID])

    if stove_state_config := config.get(CONF_STOVE_STATE):
        sens = await text_sensor.new_text_sensor(stove_state_config, mv)
        await to_code_micronova_listener(mv, sens, stove_state_config)
