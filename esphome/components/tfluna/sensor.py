import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DISTANCE,
    CONF_SIGNAL_STRENGTH,
    CONF_TEMPERATURE,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_ARROW_EXPAND_VERTICAL,
    ICON_SIGNAL,
    ICON_THERMOMETER,
    ICON_TIMER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_CENTIMETER,
    UNIT_MILLISECOND,
)

from . import CONF_TFLUNA_ID, CONF_TIMESTAMP, TFLunaComponent

DEPENDENCIES = ["tfluna"]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_TFLUNA_ID): cv.use_id(TFLunaComponent),
    cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
        unit_of_measurement=UNIT_CENTIMETER,
        icon=ICON_ARROW_EXPAND_VERTICAL,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_DISTANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        icon=ICON_THERMOMETER,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    cv.Optional(CONF_SIGNAL_STRENGTH): sensor.sensor_schema(
        icon=ICON_SIGNAL,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    cv.Optional(CONF_TIMESTAMP): sensor.sensor_schema(
        icon=ICON_TIMER,
        accuracy_decimals=0,
        unit_of_measurement=UNIT_MILLISECOND,
        device_class=DEVICE_CLASS_DURATION,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}


async def to_code(config):
    tfluna_component = await cg.get_variable(config[CONF_TFLUNA_ID])
    if distance_config := config.get(CONF_DISTANCE):
        sens = await sensor.new_sensor(distance_config)
        cg.add(tfluna_component.set_distance_sensor(sens))
    if temperature_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature_config)
        cg.add(tfluna_component.set_temperature_sensor(sens))
    if signal_strength_config := config.get(CONF_SIGNAL_STRENGTH):
        sens = await sensor.new_sensor(signal_strength_config)
        cg.add(tfluna_component.set_signal_strength_sensor(sens))
    if timestamp_config := config.get(CONF_TIMESTAMP):
        sens = await sensor.new_sensor(timestamp_config)
        cg.add(tfluna_component.set_timestamp_sensor(sens))
