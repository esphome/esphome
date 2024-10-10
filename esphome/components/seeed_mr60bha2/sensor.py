import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_DISTANCE,
    UNIT_CENTIMETER,
)
from . import CONF_MR60BHA2_ID, MR60BHA2Component

AUTO_LOAD = ["seeed_mr60bha2"]

CONF_BREATH_RATE = "breath_rate"
CONF_HEART_RATE= "heart_rate"
CONF_DISTANCE = "distance"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MR60BHA2_ID): cv.use_id(MR60BHA2Component),
        cv.Optional(CONF_BREATH_RATE): sensor.sensor_schema(
            accuracy_decimals=2,
            icon="mdi:counter",
        ),
        cv.Optional(CONF_HEART_RATE): sensor.sensor_schema(
            accuracy_decimals=2,
            icon="mdi:counter",
        ),
        cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
            device_class=DEVICE_CLASS_DISTANCE,
            unit_of_measurement=UNIT_CENTIMETER,
            accuracy_decimals=2,  # Specify the number of decimal places
            icon="mdi:signal-distance-variant",
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
