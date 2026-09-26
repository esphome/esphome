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
    UNIT_CELSIUS,
    UNIT_CENTIMETER,
    UNIT_MILLISECOND,
)
from esphome.types import ConfigType

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
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}


async def to_code(config: ConfigType) -> None:
    tfluna_component = await cg.get_variable(config[CONF_TFLUNA_ID])
    await sensor.new_sub_sensor(
        config, CONF_DISTANCE, tfluna_component.set_distance_sensor
    )
    await sensor.new_sub_sensor(
        config, CONF_TEMPERATURE, tfluna_component.set_temperature_sensor
    )
    await sensor.new_sub_sensor(
        config, CONF_SIGNAL_STRENGTH, tfluna_component.set_signal_strength_sensor
    )
    await sensor.new_sub_sensor(
        config, CONF_TIMESTAMP, tfluna_component.set_timestamp_sensor
    )
