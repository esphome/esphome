import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_HAS_TARGET, CONF_ID, DEVICE_CLASS_OCCUPANCY, ICON_ACCOUNT
from esphome.types import ConfigType

from . import CONF_DFROBOT_SEN0609_ID, DFRobotSEN0609Component

DEPENDENCIES = ["dfrobot_sen0609"]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_DFROBOT_SEN0609_ID): cv.use_id(DFRobotSEN0609Component),
    cv.Optional(CONF_HAS_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY,
        icon=ICON_ACCOUNT,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DFROBOT_SEN0609_ID])
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_HAS_TARGET, hub.set_has_target_binary_sensor)
