import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import DEVICE_CLASS_CONNECTIVITY, ENTITY_CATEGORY_DIAGNOSTIC
from . import IEC62056Component, CONF_IEC62056_ID

AUTO_LOAD = ["iec62056"]

binary_sensor_ns = cg.esphome_ns.namespace("binary_sensor")
BinarySensor = binary_sensor_ns.class_("BinarySensor", cg.EntityBase)


CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(
        BinarySensor,
        device_class=DEVICE_CLASS_CONNECTIVITY,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ).extend(
        {
            cv.GenerateID(CONF_IEC62056_ID): cv.use_id(IEC62056Component),
        }
    )
)


async def to_code(config):
    component = await cg.get_variable(config[CONF_IEC62056_ID])
    var = await binary_sensor.new_binary_sensor(config)
    cg.add(component.register_sensor(var))
