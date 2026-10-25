import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_SOURCE_ID,
)
from esphome.core.entity_helpers import inherit_property_from
from esphome.types import ConfigType

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(binary_sensor.BinarySensor).extend(
    {
        cv.Required(CONF_SOURCE_ID): cv.use_id(binary_sensor.BinarySensor),
    }
)

FINAL_VALIDATE_SCHEMA = cv.All(
    inherit_property_from(CONF_ICON, CONF_SOURCE_ID),
    inherit_property_from(CONF_ENTITY_CATEGORY, CONF_SOURCE_ID),
    inherit_property_from(CONF_DEVICE_CLASS, CONF_SOURCE_ID),
)


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    binary_sensor.follow_binary_sensor(
        var, await cg.get_variable(config[CONF_SOURCE_ID])
    )
