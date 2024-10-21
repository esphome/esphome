import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_OCCUPANCY
from . import CONF_MR60FDA2_ID, MR60FDA2Component

AUTO_LOAD = ["seeed_mr60fda2"]

CONF_PEOPLE_EXIST = "people_exist"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_MR60FDA2_ID): cv.use_id(MR60FDA2Component),
    cv.Optional(CONF_PEOPLE_EXIST): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY, icon="mdi:motion-sensor"
    ),
}


async def to_code(config):
    mr60fda2_component = await cg.get_variable(config[CONF_MR60FDA2_ID])
    if people_exist_config := config.get(CONF_PEOPLE_EXIST):
        sens = await binary_sensor.new_binary_sensor(people_exist_config)
        cg.add(mr60fda2_component.set_people_exist_binary_sensor(sens))
