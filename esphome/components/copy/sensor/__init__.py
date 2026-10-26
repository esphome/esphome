import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_SOURCE_ID,
    CONF_STATE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
)
from esphome.core.entity_helpers import inherit_property_from
from esphome.types import ConfigType

CONFIG_SCHEMA = sensor.sensor_schema(sensor.Sensor).extend(
    {
        cv.Required(CONF_SOURCE_ID): cv.use_id(sensor.Sensor),
    }
)

FINAL_VALIDATE_SCHEMA = cv.All(
    inherit_property_from(CONF_UNIT_OF_MEASUREMENT, CONF_SOURCE_ID),
    inherit_property_from(CONF_ICON, CONF_SOURCE_ID),
    inherit_property_from(CONF_ACCURACY_DECIMALS, CONF_SOURCE_ID),
    inherit_property_from(CONF_DEVICE_CLASS, CONF_SOURCE_ID),
    inherit_property_from(CONF_STATE_CLASS, CONF_SOURCE_ID),
    inherit_property_from(CONF_ENTITY_CATEGORY, CONF_SOURCE_ID),
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    sensor.follow_sensor(var, await cg.get_variable(config[CONF_SOURCE_ID]))
