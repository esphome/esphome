import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_HAS_TARGET, DEVICE_CLASS_OCCUPANCY
from esphome.types import ConfigType

from . import CONF_MR60BHA2_ID, MR60BHA2Component

DEPENDENCIES = ["seeed_mr60bha2"]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_MR60BHA2_ID): cv.use_id(MR60BHA2Component),
    cv.Optional(CONF_HAS_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY, icon="mdi:motion-sensor"
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MR60BHA2_ID])

    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_HAS_TARGET, hub.set_has_target_binary_sensor)
