import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_CONNECTIVITY, ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_SIM800L_ID, Sim800LComponent

DEPENDENCIES = ["sim800l"]

CONF_REGISTERED = "registered"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_SIM800L_ID): cv.use_id(Sim800LComponent),
    cv.Optional(CONF_REGISTERED): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_CONNECTIVITY,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}


async def to_code(config):
    hub = await cg.get_variable(config[CONF_SIM800L_ID])

    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_REGISTERED, hub.set_registered_binary_sensor)
