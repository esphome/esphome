import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_OCCUPANCY, DEVICE_CLASS_SAFETY
from esphome.types import ConfigType

from . import CONF_MR60FDA2_ID, MR60FDA2Component

DEPENDENCIES = ["seeed_mr60fda2"]

CONF_PEOPLE_EXIST = "people_exist"
CONF_FALL_DETECTED = "fall_detected"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_MR60FDA2_ID): cv.use_id(MR60FDA2Component),
    cv.Optional(CONF_PEOPLE_EXIST): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY, icon="mdi:motion-sensor"
    ),
    cv.Optional(CONF_FALL_DETECTED): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_SAFETY, icon="mdi:emergency"
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MR60FDA2_ID])

    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_PEOPLE_EXIST, hub.set_people_exist_binary_sensor)
    await binary_sensors(CONF_FALL_DETECTED, hub.set_fall_detected_binary_sensor)
