import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ANGLE,
    CONF_DISTANCE,
    CONF_RESOLUTION,
    CONF_SPEED,
    CONF_X,
    CONF_Y,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_SPEED,
    STATE_CLASS_MEASUREMENT,
    UNIT_DEGREES,
    UNIT_MILLIMETER,
)

from . import CONF_RD03D_ID, RD03DComponent

DEPENDENCIES = ["rd03d"]

CONF_TARGET_COUNT = "target_count"

MAX_TARGETS = 3

UNIT_MILLIMETER_PER_SECOND = "mm/s"

TARGET_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_X): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILLIMETER,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_Y): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILLIMETER,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SPEED): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILLIMETER_PER_SECOND,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SPEED,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILLIMETER,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_RESOLUTION): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILLIMETER,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ANGLE): sensor.sensor_schema(
            unit_of_measurement=UNIT_DEGREES,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_RD03D_ID): cv.use_id(RD03DComponent),
        cv.Optional(CONF_TARGET_COUNT): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
).extend({cv.Optional(f"target_{i + 1}"): TARGET_SCHEMA for i in range(MAX_TARGETS)})


async def to_code(config):
    hub = await cg.get_variable(config[CONF_RD03D_ID])

    if target_count_config := config.get(CONF_TARGET_COUNT):
        sens = await sensor.new_sensor(target_count_config)
        cg.add(hub.set_target_count_sensor(sens))

    for i in range(MAX_TARGETS):
        if target_config := config.get(f"target_{i + 1}"):
            if x_config := target_config.get(CONF_X):
                sens = await sensor.new_sensor(x_config)
                cg.add(hub.set_x_sensor(i, sens))
            if y_config := target_config.get(CONF_Y):
                sens = await sensor.new_sensor(y_config)
                cg.add(hub.set_y_sensor(i, sens))
            if speed_config := target_config.get(CONF_SPEED):
                sens = await sensor.new_sensor(speed_config)
                cg.add(hub.set_speed_sensor(i, sens))
            if distance_config := target_config.get(CONF_DISTANCE):
                sens = await sensor.new_sensor(distance_config)
                cg.add(hub.set_distance_sensor(i, sens))
            if resolution_config := target_config.get(CONF_RESOLUTION):
                sens = await sensor.new_sensor(resolution_config)
                cg.add(hub.set_resolution_sensor(i, sens))
            if angle_config := target_config.get(CONF_ANGLE):
                sens = await sensor.new_sensor(angle_config)
                cg.add(hub.set_angle_sensor(i, sens))
