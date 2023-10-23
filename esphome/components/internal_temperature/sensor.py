import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.components.esp32 import get_esp32_variant
from esphome.components.esp32.const import (
    VARIANT_ESP32S3,
)
from esphome.const import (
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    PLATFORM_ESP32,
    PLATFORM_RP2040,
)
from esphome.core import CORE

internal_temperature_ns = cg.esphome_ns.namespace("internal_temperature")
InternalTemperatureSensor = internal_temperature_ns.class_(
    "InternalTemperatureSensor", sensor.Sensor, cg.PollingComponent
)


def validate_config(config):
    if CORE.is_esp32:
        variant = get_esp32_variant()
        if variant == VARIANT_ESP32S3:
            if CORE.using_arduino and CORE.data[KEY_CORE][
                KEY_FRAMEWORK_VERSION
            ] < cv.Version(2, 0, 6):
                raise cv.Invalid(
                    "ESP32-S3 Internal Temperature Sensor requires framework version 2.0.6 or higher. See <https://github.com/esphome/issues/issues/4271>."
                )
            if CORE.using_esp_idf and CORE.data[KEY_CORE][
                KEY_FRAMEWORK_VERSION
            ] < cv.Version(4, 4, 3):
                raise cv.Invalid(
                    "ESP32-S3 Internal Temperature Sensor requires framework version 4.4.3 or higher. See <https://github.com/esphome/issues/issues/4271>."
                )
    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        InternalTemperatureSensor,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ).extend(cv.polling_component_schema("60s")),
    cv.only_on([PLATFORM_ESP32, PLATFORM_RP2040]),
    validate_config,
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
