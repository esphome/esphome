import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_OCCUPANCY

from . import AMG8833, CONF_AMG8833_ID

DEPENDENCIES = ["amg8833"]

CONF_MOTION = "motion"
CONF_PRESENCE = "presence"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_AMG8833_ID): cv.use_id(AMG8833),
    cv.Optional(CONF_MOTION): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY, icon="mdi:motion-sensor"
    ),
    cv.Optional(CONF_PRESENCE): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY, icon="mdi:motion-sensor"
    ),
}


async def to_code(config):
    amg8833_component = await cg.get_variable(config[CONF_AMG8833_ID])
    if motion := config.get(CONF_MOTION):
        sens = await binary_sensor.new_binary_sensor(motion)
        cg.add(amg8833_component.set_motion_binary_sensor(sens))
    if presence := config.get(CONF_PRESENCE):
        sens = await binary_sensor.new_binary_sensor(presence)
        cg.add(amg8833_component.set_presence_binary_sensor(sens))
