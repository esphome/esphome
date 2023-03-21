import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

internal_temperature_ns = cg.esphome_ns.namespace("internal_temperature")
InternalTemperatureSensor = internal_temperature_ns.class_(
    "InternalTemperatureSensor", sensor.Sensor, cg.PollingComponent
)

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        InternalTemperatureSensor,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ).extend(cv.polling_component_schema("60s")),
    cv.only_on(["esp32", "rp2040"]),
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
