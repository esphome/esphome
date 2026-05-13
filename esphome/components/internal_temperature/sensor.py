import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.zephyr import zephyr_add_prj_conf
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_LN882X,
    PLATFORM_NRF52,
    PLATFORM_RP2040,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    PlatformFramework,
)
from esphome.core import CORE

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
    cv.only_on(
        [
            PLATFORM_ESP32,
            PLATFORM_RP2040,
            PLATFORM_BK72XX,
            PLATFORM_NRF52,
            PLATFORM_LN882X,
        ]
    ),
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    if CORE.using_zephyr and CORE.is_nrf52:
        zephyr_add_prj_conf("SENSOR", True)
        zephyr_add_prj_conf("TEMP_NRF5", True)


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "internal_temperature_esp32.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        "internal_temperature_rp2040.cpp": {PlatformFramework.RP2040_ARDUINO},
        "internal_temperature_bk72xx.cpp": {
            PlatformFramework.BK72XX_ARDUINO,
        },
        "internal_temperature_ln882x.cpp": {
            PlatformFramework.LN882X_ARDUINO,
        },
        "internal_temperature_zephyr.cpp": {PlatformFramework.NRF52_ZEPHYR},
    }
)
