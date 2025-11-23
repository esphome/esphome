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
    sim800l_component = await cg.get_variable(config[CONF_SIM800L_ID])

    if CONF_REGISTERED in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_REGISTERED])
        cg.add(sim800l_component.set_registered_binary_sensor(sens))
