import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ds248x, sensor
from esphome.const import (
    CONF_RESOLUTION,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

DEPENDENCIES = ["ds248x"]

ds18x_ns = cg.esphome_ns.namespace("ds18x")
DS18xTemperatureSensor = ds18x_ns.class_("DS18xTemperatureSensor", ds248x.DS248xSensor)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        DS18xTemperatureSensor,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend({cv.Optional(CONF_RESOLUTION, default=12): cv.int_range(min=9, max=12)})
    .extend(ds248x.ds248x_sensor_schema())
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    # var = cg.new_Pvariable(config[CONF_ID])
    # await cg.register_component(var, config)
    await ds248x.register_ds248x_sensor(var, config)

    if CONF_RESOLUTION in config:
        cg.add(var.set_resolution(config[CONF_RESOLUTION]))
