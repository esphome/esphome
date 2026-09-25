import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_HAS_TARGET, CONF_ID, DEVICE_CLASS_OCCUPANCY
from esphome.types import ConfigType

from . import CONF_LD2460_ID, LD2460Component

DEPENDENCIES = ["ld2460"]

ICON_SHIELD_ACCOUNT = "mdi:shield-account"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_LD2460_ID): cv.use_id(LD2460Component),
    cv.Optional(CONF_HAS_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY,
        icon=ICON_SHIELD_ACCOUNT,
    ),
}


async def to_code(config: ConfigType) -> None:
    ld2460_component = await cg.get_variable(config[CONF_LD2460_ID])
    if has_target_config := config.get(CONF_HAS_TARGET):
        sens = await binary_sensor.new_binary_sensor(has_target_config)
        cg.add(ld2460_component.set_target_binary_sensor(sens))
