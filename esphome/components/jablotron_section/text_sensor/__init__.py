import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.const import CONF_ID
from esphome.components.jablotron import (
    CONF_INDEX,
    INDEX_SCHEMA,
    JABLOTRON_DEVICE_SCHEMA,
    register_jablotron_device,
)
from esphome.components.jablotron_section import jablotron_section_ns

SectionSensor = jablotron_section_ns.class_("SectionSensor", text_sensor.TextSensor)

DEPENDENCIES = ["jablotron"]
CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(SectionSensor)
    .extend(JABLOTRON_DEVICE_SCHEMA)
    .extend(INDEX_SCHEMA)
)


async def to_code(config):
    sensor = cg.new_Pvariable(config[CONF_ID])
    cg.add(sensor.set_index(config[CONF_INDEX]))
    await register_jablotron_device(sensor, config)
    await text_sensor.register_text_sensor(sensor, config)
