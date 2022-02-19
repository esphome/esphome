import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_SECOND,
    ICON_TIMER,
)

uptime_ns = cg.esphome_ns.namespace("uptime")
UptimeSensor = uptime_ns.class_("UptimeSensor", sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = sensor.sensor_schema(
    UptimeSensor,
    unit_of_measurement=UNIT_SECOND,
    icon=ICON_TIMER,
    accuracy_decimals=0,
    state_class=STATE_CLASS_TOTAL_INCREASING,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
