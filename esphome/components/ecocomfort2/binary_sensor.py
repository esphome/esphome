import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.components.const import CONF_CONNECTED
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_CONNECTIVITY, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from . import CONF_ECOCOMFORT2_ID, ECOCOMFORT2_CLIENT_SCHEMA

CODEOWNERS = ["@gledian"]
DEPENDENCIES = ["ecocomfort2"]

CONF_BOOST = "boost"

CONFIG_SCHEMA = cv.All(
    ECOCOMFORT2_CLIENT_SCHEMA.extend(
        {
            cv.Optional(CONF_CONNECTED): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_CONNECTIVITY,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_BOOST): binary_sensor.binary_sensor_schema(),
        }
    ),
    cv.has_at_least_one_key(CONF_CONNECTED, CONF_BOOST),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ECOCOMFORT2_ID])
    sensors = binary_sensor.sub_binary_sensors(config)
    await sensors(CONF_CONNECTED, hub.set_connected_binary_sensor)
    await sensors(CONF_BOOST, hub.set_boost_binary_sensor)
