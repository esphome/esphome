import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_CONNECTIVITY, ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_SIM7600_ID, Sim7600Component

DEPENDENCIES = ["sim7600"]

CONF_REGISTERED = "registered"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_SIM7600_ID): cv.use_id(Sim7600Component),
    cv.Optional(CONF_REGISTERED): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_CONNECTIVITY,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}


async def to_code(config):
    sim7600_component = await cg.get_variable(config[CONF_SIM7600_ID])

    if CONF_REGISTERED in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_REGISTERED])
        cg.add(sim7600_component.set_registered_binary_sensor(sens))
