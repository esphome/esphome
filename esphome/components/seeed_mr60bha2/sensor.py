import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DISTANCE,
    DEVICE_CLASS_DISTANCE,
    ICON_COUNTER,
    ICON_HEART_PULSE,
    ICON_PULSE,
    ICON_SIGNAL,
    STATE_CLASS_MEASUREMENT,
    UNIT_BEATS_PER_MINUTE,
    UNIT_CENTIMETER,
)

from . import CONF_MR60BHA2_ID, MR60BHA2Component

DEPENDENCIES = ["seeed_mr60bha2"]

CONF_BREATH_RATE = "breath_rate"
CONF_HEART_RATE = "heart_rate"
CONF_NUM_TARGETS = "num_targets"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MR60BHA2_ID): cv.use_id(MR60BHA2Component),
        cv.Optional(CONF_BREATH_RATE): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_PULSE,
        ),
        cv.Optional(CONF_HEART_RATE): sensor.sensor_schema(
            accuracy_decimals=0,
            icon=ICON_HEART_PULSE,
            state_class=STATE_CLASS_MEASUREMENT,
            unit_of_measurement=UNIT_BEATS_PER_MINUTE,
        ),
        cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
            unit_of_measurement=UNIT_CENTIMETER,
            accuracy_decimals=2,
            icon=ICON_SIGNAL,
        ),
        cv.Optional(CONF_NUM_TARGETS): sensor.sensor_schema(
            icon=ICON_COUNTER,
        ),
    }
)


async def to_code(config):
    mr60bha2_component = await cg.get_variable(config[CONF_MR60BHA2_ID])
    if breath_rate_config := config.get(CONF_BREATH_RATE):
        sens = await sensor.new_sensor(breath_rate_config)
        cg.add(mr60bha2_component.set_breath_rate_sensor(sens))
    if heart_rate_config := config.get(CONF_HEART_RATE):
        sens = await sensor.new_sensor(heart_rate_config)
        cg.add(mr60bha2_component.set_heart_rate_sensor(sens))
    if distance_config := config.get(CONF_DISTANCE):
        sens = await sensor.new_sensor(distance_config)
        cg.add(mr60bha2_component.set_distance_sensor(sens))
    if num_targets_config := config.get(CONF_NUM_TARGETS):
        sens = await sensor.new_sensor(num_targets_config)
        cg.add(mr60bha2_component.set_num_targets_sensor(sens))
