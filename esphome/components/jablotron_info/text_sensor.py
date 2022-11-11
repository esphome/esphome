import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.const import CONF_ID, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.components.jablotron import (
    register_jablotron_device,
    JABLOTRON_DEVICE_SCHEMA,
)

DEPENDENCIES = ["jablotron", "text_sensor"]

jablotron_info_ns = cg.esphome_ns.namespace("jablotron_info")
InfoSensor = jablotron_info_ns.class_("InfoSensor", text_sensor.TextSensor)

CONFIG_SCHEMA = text_sensor.text_sensor_schema(
    InfoSensor, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
).extend(JABLOTRON_DEVICE_SCHEMA)


async def to_code(config):
    sensor = cg.new_Pvariable(config[CONF_ID])
    await register_jablotron_device(sensor, config)
    await text_sensor.register_text_sensor(sensor, config)
