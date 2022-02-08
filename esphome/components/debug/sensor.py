import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_FREE,
    CONF_FRAGMENTATION,
    CONF_BLOCK,
    CONF_LOOP_TIME,
    UNIT_MILLISECOND,
    UNIT_PERCENT,
    UNIT_BYTES,
    ICON_COUNTER,
    ICON_TIMER,
)
from . import CONF_DEBUG_ID, DebugComponent

DEPENDENCIES = ["debug"]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_DEBUG_ID): cv.use_id(DebugComponent),
    cv.Optional(CONF_FREE): sensor.sensor_schema(UNIT_BYTES, ICON_COUNTER, 0),
    cv.Optional(CONF_BLOCK): sensor.sensor_schema(UNIT_BYTES, ICON_COUNTER, 0),
    cv.Optional(CONF_FRAGMENTATION): cv.All(
        cv.only_on_esp8266,
        cv.require_framework_version(esp8266_arduino=cv.Version(2, 5, 2)),
        sensor.sensor_schema(UNIT_PERCENT, ICON_COUNTER, 1),
    ),
    cv.Optional(CONF_LOOP_TIME): sensor.sensor_schema(UNIT_MILLISECOND, ICON_TIMER, 0),
}


async def to_code(config):
    debug_component = await cg.get_variable(config[CONF_DEBUG_ID])

    if CONF_FREE in config:
        sens = await sensor.new_sensor(config[CONF_FREE])
        cg.add(debug_component.set_free_sensor(sens))

    if CONF_BLOCK in config:
        sens = await sensor.new_sensor(config[CONF_BLOCK])
        cg.add(debug_component.set_block_sensor(sens))

    if CONF_FRAGMENTATION in config:
        sens = await sensor.new_sensor(config[CONF_FRAGMENTATION])
        cg.add(debug_component.set_fragmentation_sensor(sens))

    if CONF_LOOP_TIME in config:
        sens = await sensor.new_sensor(config[CONF_LOOP_TIME])
        cg.add(debug_component.set_loop_time_sensor(sens))
