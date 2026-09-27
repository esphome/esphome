import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.esp32 import CONF_CPU_FREQUENCY
from esphome.components.psram import DOMAIN as PSRAM_DOMAIN
import esphome.config_validation as cv
from esphome.const import (
    CONF_BLOCK,
    CONF_FRAGMENTATION,
    CONF_FREE,
    CONF_LOOP_TIME,
    DEVICE_CLASS_FREQUENCY,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_COUNTER,
    ICON_TIMER,
    PLATFORM_BK72XX,
    PLATFORM_LN882X,
    PLATFORM_RTL87XX,
    STATE_CLASS_MEASUREMENT,
    UNIT_BYTES,
    UNIT_HERTZ,
    UNIT_MILLISECOND,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

from . import CONF_DEBUG_ID, FILTER_SOURCE_FILES, DebugComponent  # noqa: F401  pylint: disable=unused-import

DEPENDENCIES = ["debug"]

CONF_MIN_FREE = "min_free"
CONF_PSRAM = "psram"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_DEBUG_ID): cv.use_id(DebugComponent),
    cv.Optional(CONF_FREE): sensor.sensor_schema(
        unit_of_measurement=UNIT_BYTES,
        icon=ICON_COUNTER,
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_BLOCK): sensor.sensor_schema(
        unit_of_measurement=UNIT_BYTES,
        icon=ICON_COUNTER,
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_FRAGMENTATION): cv.All(
        cv.Any(
            cv.only_on_esp8266,
            cv.only_on_esp32,
            msg="This feature is only available on ESP8266 and ESP32",
        ),
        sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            icon=ICON_COUNTER,
            accuracy_decimals=1,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    ),
    cv.Optional(CONF_MIN_FREE): cv.All(
        cv.Any(
            cv.only_on_esp32,
            cv.only_on([PLATFORM_BK72XX, PLATFORM_LN882X, PLATFORM_RTL87XX]),
            msg="This feature is only available on ESP32 and LibreTiny (BK72xx, LN882x, RTL87xx)",
        ),
        sensor.sensor_schema(
            unit_of_measurement=UNIT_BYTES,
            icon=ICON_COUNTER,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    ),
    cv.Optional(CONF_LOOP_TIME): sensor.sensor_schema(
        unit_of_measurement=UNIT_MILLISECOND,
        icon=ICON_TIMER,
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_PSRAM): cv.All(
        cv.only_on_esp32,
        cv.requires_component(PSRAM_DOMAIN),
        sensor.sensor_schema(
            unit_of_measurement=UNIT_BYTES,
            icon=ICON_COUNTER,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    ),
    cv.Optional(CONF_CPU_FREQUENCY): cv.All(
        sensor.sensor_schema(
            unit_of_measurement=UNIT_HERTZ,
            icon="mdi:speedometer",
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_FREQUENCY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    ),
}


async def to_code(config: ConfigType) -> None:
    debug_component = await cg.get_variable(config[CONF_DEBUG_ID])

    await sensor.new_sub_sensor(config, CONF_FREE, debug_component.set_free_sensor)
    await sensor.new_sub_sensor(config, CONF_BLOCK, debug_component.set_block_sensor)
    await sensor.new_sub_sensor(
        config, CONF_FRAGMENTATION, debug_component.set_fragmentation_sensor
    )
    await sensor.new_sub_sensor(
        config, CONF_MIN_FREE, debug_component.set_min_free_sensor
    )
    await sensor.new_sub_sensor(
        config, CONF_LOOP_TIME, debug_component.set_loop_time_sensor
    )
    await sensor.new_sub_sensor(config, CONF_PSRAM, debug_component.set_psram_sensor)
    await sensor.new_sub_sensor(
        config, CONF_CPU_FREQUENCY, debug_component.set_cpu_frequency_sensor
    )
