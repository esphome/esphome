import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_EMPTY,
    UNIT_PERCENT,
)
from . import CONF_MR24D11C10_ID, MR24D11C10Component

DEPENDENCIES = ["mr24d11c10"]
CONF_BODY_MOVEMENT = "body_movement"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_MR24D11C10_ID): cv.use_id(MR24D11C10Component),
    cv.Optional(CONF_BODY_MOVEMENT): sensor.sensor_schema(
        device_class=DEVICE_CLASS_EMPTY, unit_of_measurement=UNIT_PERCENT
    ),
}


async def to_code(config):
    mr24d11c10_component = await cg.get_variable(config[CONF_MR24D11C10_ID])
    if CONF_BODY_MOVEMENT in config:
        sens = await sensor.new_sensor(config[CONF_BODY_MOVEMENT])
        cg.add(mr24d11c10_component.set_body_movement_sensor(sens))
