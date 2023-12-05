import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_OCCUPANCY,  # https://github.com/limengdu/esphome/blob/17e1d4c2455997a0a15c875950be1942c6249d0d/esphome/const.py#L1006
)
from . import CONF_MR24HPC1_ID, mr24hpc1Component

AUTO_LOAD = ["mr24hpc1"]
CONF_SOMEONEEXIST = "someone_exist"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_MR24HPC1_ID): cv.use_id(mr24hpc1Component),
    cv.Optional(CONF_SOMEONEEXIST): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY, icon="mdi:motion-sensor"
    ),
}


async def to_code(config):
    mr24hpc1_component = await cg.get_variable(config[CONF_MR24HPC1_ID])
    if someoneexists_config := config.get(CONF_SOMEONEEXIST):
        sens = await binary_sensor.new_binary_sensor(someoneexists_config)
        cg.add(mr24hpc1_component.set_someoneExists_binary_sensor(sens))
