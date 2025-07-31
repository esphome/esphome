import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.esp32 import CONF_CPU_FREQUENCY
import esphome.config_validation as cv
from esphome.const import (
    CONF_BLOCK,
    CONF_FRAGMENTATION,
    CONF_FREE,
    CONF_LOOP_TIME,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_COUNTER,
    ICON_TIMER,
    UNIT_BYTES,
    UNIT_HERTZ,
    UNIT_MILLISECOND,
    UNIT_PERCENT,
)

from . import CONF_DEBUG_ID, DebugComponent

DEPENDENCIES = ["debug"]

CONF_PSRAM = "psram"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_DEBUG_ID): cv.use_id(DebugComponent),
    cv.Optional(CONF_FREE): sensor.sensor_schema(
        unit_of_measurement=UNIT_BYTES,
        icon=ICON_COUNTER,
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    cv.Optional(CONF_BLOCK): sensor.sensor_schema(
        unit_of_measurement=UNIT_BYTES,
        icon=ICON_COUNTER,
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    cv.Optional(CONF_FRAGMENTATION): cv.All(
        cv.only_on_esp8266,
        cv.require_framework_version(esp8266_arduino=cv.Version(2, 5, 2)),
        sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            icon=ICON_COUNTER,
            accuracy_decimals=1,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    ),
    cv.Optional(CONF_LOOP_TIME): sensor.sensor_schema(
        unit_of_measurement=UNIT_MILLISECOND,
        icon=ICON_TIMER,
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    cv.Optional(CONF_PSRAM): cv.All(
        cv.only_on_esp32,
        cv.requires_component("psram"),
        sensor.sensor_schema(
            unit_of_measurement=UNIT_BYTES,
            icon=ICON_COUNTER,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    ),
    cv.Optional(CONF_CPU_FREQUENCY): cv.All(
        sensor.sensor_schema(
            unit_of_measurement=UNIT_HERTZ,
            icon="mdi:speedometer",
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    ),
}


async def to_code(config):
    debug_component = await cg.get_variable(config[CONF_DEBUG_ID])

    if free_conf := config.get(CONF_FREE):
        sens = await sensor.new_sensor(free_conf)
        cg.add(debug_component.set_free_sensor(sens))

    if block_conf := config.get(CONF_BLOCK):
        sens = await sensor.new_sensor(block_conf)
        cg.add(debug_component.set_block_sensor(sens))

    if fragmentation_conf := config.get(CONF_FRAGMENTATION):
        sens = await sensor.new_sensor(fragmentation_conf)
        cg.add(debug_component.set_fragmentation_sensor(sens))

    if loop_time_conf := config.get(CONF_LOOP_TIME):
        sens = await sensor.new_sensor(loop_time_conf)
        cg.add(debug_component.set_loop_time_sensor(sens))

    if psram_conf := config.get(CONF_PSRAM):
        sens = await sensor.new_sensor(psram_conf)
        cg.add(debug_component.set_psram_sensor(sens))

    if cpu_freq_conf := config.get(CONF_CPU_FREQUENCY):
        sens = await sensor.new_sensor(cpu_freq_conf)
        cg.add(debug_component.set_cpu_frequency_sensor(sens))
