import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_MOTION

from . import CONF_DFROBOT_SEN0395_ID, DfrobotSen0395Component

DEPENDENCIES = ["dfrobot_sen0395"]

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class=DEVICE_CLASS_MOTION
).extend(
    {
        cv.GenerateID(CONF_DFROBOT_SEN0395_ID): cv.use_id(DfrobotSen0395Component),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DFROBOT_SEN0395_ID])
    binary_sens = await binary_sensor.new_binary_sensor(config)

    cg.add(parent.set_detected_binary_sensor(binary_sens))
