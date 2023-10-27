import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ANGLE,
    CONF_RESOLUTION,
    CONF_SPEED,
    CONF_X,
    CONF_Y,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_SPEED,
    ICON_ANGLE_ACUTE,
    ICON_ARTBOARD,
    ICON_COUNTER,
    ICON_FOCUS_FIELD_HORIZONTAL,
    ICON_FOCUS_FIELD_VERTICAL,
    STATE_CLASS_MEASUREMENT,
)
from . import CONF_LD2450_ID, LD2450Component, NUM_TARGETS

DEPENDENCIES = ["ld2450"]

CONF_ANY_PRESENCE = "any_presence"
CONF_ALL_TARGET_COUNTS = "all_target_counts"


config_schema = {
    cv.GenerateID(CONF_LD2450_ID): cv.use_id(LD2450Component),
    cv.Optional(CONF_ALL_TARGET_COUNTS): sensor.sensor_schema(
        accuracy_decimals=0,
        icon=ICON_COUNTER,
        unit_of_measurement="targets",
    ),
    **{
        cv.Optional(f"target{n}"): {
            cv.Optional(CONF_ANGLE): sensor.sensor_schema(
                accuracy_decimals=1,
                icon=ICON_ANGLE_ACUTE,
                state_class=STATE_CLASS_MEASUREMENT,
                unit_of_measurement="°",
            ),
            cv.Optional(CONF_X): sensor.sensor_schema(
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DISTANCE,
                icon=ICON_FOCUS_FIELD_HORIZONTAL,
                state_class=STATE_CLASS_MEASUREMENT,
                unit_of_measurement="mm",
            ),
            cv.Optional(CONF_Y): sensor.sensor_schema(
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DISTANCE,
                icon=ICON_FOCUS_FIELD_VERTICAL,
                state_class=STATE_CLASS_MEASUREMENT,
                unit_of_measurement="mm",
            ),
            cv.Optional(CONF_SPEED): sensor.sensor_schema(
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_SPEED,
                state_class=STATE_CLASS_MEASUREMENT,
                unit_of_measurement="cm/s",
            ),
            cv.Optional(CONF_RESOLUTION): sensor.sensor_schema(
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DISTANCE,
                icon=ICON_ARTBOARD,
                state_class=STATE_CLASS_MEASUREMENT,
                unit_of_measurement="mm",
            ),
        }
        for n in range(NUM_TARGETS)
    },
}
CONFIG_SCHEMA = cv.Schema(config_schema)


async def to_code(config):
    ld2450_component = await cg.get_variable(config[CONF_LD2450_ID])
    if all_target_counts := config.get(CONF_ALL_TARGET_COUNTS):
        sens = await sensor.new_sensor(all_target_counts)
        cg.add(ld2450_component.set_all_target_counts_sensor(sens))

    for n in range(NUM_TARGETS):
        if target_conf := config.get(f"target{n}"):
            if x_config := target_conf.get(CONF_X):
                sens = await sensor.new_sensor(x_config)
                cg.add(ld2450_component.get_target(n).set_x_sensor(sens))
            if y_config := target_conf.get(CONF_Y):
                sens = await sensor.new_sensor(y_config)
                cg.add(ld2450_component.get_target(n).set_y_sensor(sens))
            if angle_config := target_conf.get(CONF_ANGLE):
                sens = await sensor.new_sensor(angle_config)
                cg.add(ld2450_component.get_target(n).set_angle_sensor(sens))
            if speed_config := target_conf.get(CONF_SPEED):
                sens = await sensor.new_sensor(speed_config)
                cg.add(ld2450_component.get_target(n).set_speed_sensor(sens))
            if resolution_config := target_conf.get(CONF_RESOLUTION):
                sens = await sensor.new_sensor(resolution_config)
                cg.add(ld2450_component.get_target(n).set_resolution_sensor(sens))
