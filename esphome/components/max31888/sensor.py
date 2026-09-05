import esphome.codegen as cg
from esphome.components import one_wire, sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

max31888_ns = cg.esphome_ns.namespace("max31888")

MAX31888Sensor = max31888_ns.class_(
    "MAX31888Sensor",
    cg.PollingComponent,
    sensor.Sensor,
    one_wire.OneWireDevice,
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        MAX31888Sensor,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(one_wire.one_wire_device_schema())
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await one_wire.register_one_wire_device(var, config)
