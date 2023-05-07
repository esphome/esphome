import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from . import CONF_MR24D11C10_ID, MR24D11C10Component

DEPENDENCIES = ["mr24d11c10"]
CONF_HUMAN_PRESENCE = "human_presence"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_MR24D11C10_ID): cv.use_id(MR24D11C10Component),
    cv.Optional(CONF_HUMAN_PRESENCE): binary_sensor.binary_sensor_schema(),
}


async def to_code(config):
    mr24d11c10_component = await cg.get_variable(config[CONF_MR24D11C10_ID])
    if CONF_HUMAN_PRESENCE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HUMAN_PRESENCE])
        cg.add(mr24d11c10_component.set_human_presence_binary_sensor(sens))
