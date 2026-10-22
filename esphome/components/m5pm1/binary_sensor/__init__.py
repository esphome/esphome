import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import BASE_SCHEMA, CONF_M5PM1_ID, m5pm1_ns

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["m5pm1"]

M5PM1PowerButtonBinarySensor = m5pm1_ns.class_(
    "M5PM1PowerButtonBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(M5PM1PowerButtonBinarySensor)
    .extend(BASE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_M5PM1_ID])
    var = await binary_sensor.new_binary_sensor(config, parent)
    await cg.register_component(var, config)
