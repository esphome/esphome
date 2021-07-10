import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_EMPTY,
    STATE_CLASS_NONE,
    UNIT_SECOND,
    ICON_TIMER,
)

uptime_ns = cg.esphome_ns.namespace("uptime")
UptimeSensor = uptime_ns.class_("UptimeSensor", sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        UNIT_SECOND, ICON_TIMER, 0, DEVICE_CLASS_EMPTY, STATE_CLASS_NONE
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(UptimeSensor),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
