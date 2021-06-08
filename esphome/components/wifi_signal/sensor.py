import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ICON_EMPTY,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
)

DEPENDENCIES = ["wifi"]
wifi_signal_ns = cg.esphome_ns.namespace("wifi_signal")
WiFiSignalSensor = wifi_signal_ns.class_(
    "WiFiSignalSensor", sensor.Sensor, cg.PollingComponent
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        UNIT_DECIBEL_MILLIWATT,
        ICON_EMPTY,
        0,
        DEVICE_CLASS_SIGNAL_STRENGTH,
        STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(WiFiSignalSensor),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
