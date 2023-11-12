import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import sensor
from . import CONF_MR24HPB1_ID, MR24HPB1Component

DEPENDENCIES = ["mr24hpb1"]

# Movement rate
CONF_MOVEMENT_RATE = "movement_rate"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MR24HPB1_ID): cv.use_id(MR24HPB1Component),
        cv.Optional(CONF_MOVEMENT_RATE): sensor.sensor_schema(),
    }
)


async def to_code(config):
    var = await cg.get_variable(config[CONF_MR24HPB1_ID])

    if CONF_MOVEMENT_RATE in config:
        sens = await sensor.new_sensor(config[CONF_MOVEMENT_RATE])
        cg.add(var.set_movement_rate_sensor(sens))
