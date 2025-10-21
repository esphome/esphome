import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_RP2040,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
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
    cv.only_on([PLATFORM_ESP32, PLATFORM_RP2040, PLATFORM_BK72XX]),
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
