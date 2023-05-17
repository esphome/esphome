import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_OCCUPANCY
from . import CONF_LD2420_ID, LD2420Component

DEPENDENCIES = ["ld2420"]
CONF_HAS_TARGET = "has_target"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2420_ID): cv.use_id(LD2420Component),
    cv.Optional(CONF_HAS_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY
    ),
}


async def to_code(config):
    ld2420_component = await cg.get_variable(config[CONF_LD2420_ID])
    if CONF_HAS_TARGET in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HAS_TARGET])
        cg.add(ld2420_component.set_presence_sensor(sens))
