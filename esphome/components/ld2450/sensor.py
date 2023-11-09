import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_SPEED,
    UNIT_DEGREES,
    CONF_SPEED,
    CONF_DISTANCE,
    CONF_RESOLUTION,
)
from . import CONF_LD2450_ID, LD2450Component

DEPENDENCIES = ["ld2450"]

UNIT_MILLIMETER = "mm"
UNIT_MILLIMETER_PER_SECOND = "mm/s"

CONF_TARGET_COUNT = "target_count"
CONF_STILL_TARGET_COUNT = "still_target_count"
CONF_MOVING_TARGET_COUNT = "moving_target_count"

MAX_TARGETS = 3
MAX_ZONES = 3

CONF_X = "x"
CONF_Y = "y"
CONF_ANGLE = "angle"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD2450_ID): cv.use_id(LD2450Component),
        cv.Optional(CONF_TARGET_COUNT): sensor.sensor_schema(
            icon="mdi:account-group",
        ),
        cv.Optional(CONF_STILL_TARGET_COUNT): sensor.sensor_schema(
            icon="mdi:human-greeting-proximity",
        ),
        cv.Optional(CONF_MOVING_TARGET_COUNT): sensor.sensor_schema(
            icon="mdi:account-switch",
        ),
    }
)

CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
    {
        cv.Optional(f"target_{n+1}"): cv.Schema(
            {
                cv.Optional(CONF_X): sensor.sensor_schema(
                    device_class=DEVICE_CLASS_DISTANCE,
                    unit_of_measurement=UNIT_MILLIMETER,
                    icon="mdi:alpha-x-box-outline",
                ),
                cv.Optional(CONF_Y): sensor.sensor_schema(
                    device_class=DEVICE_CLASS_DISTANCE,
                    unit_of_measurement=UNIT_MILLIMETER,
                    icon="mdi:alpha-y-box-outline",
                ),
                cv.Optional(CONF_SPEED): sensor.sensor_schema(
                    device_class=DEVICE_CLASS_SPEED,
                    unit_of_measurement=UNIT_MILLIMETER_PER_SECOND,
                    icon="mdi:speedometer-slow",
                ),
                cv.Optional(CONF_ANGLE): sensor.sensor_schema(
                    unit_of_measurement=UNIT_DEGREES,
                    icon="mdi:format-text-rotation-angle-up",
                ),
                cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
                    device_class=DEVICE_CLASS_DISTANCE,
                    unit_of_measurement=UNIT_MILLIMETER,
                    icon="mdi:map-marker-distance",
                ),
                cv.Optional(CONF_RESOLUTION): sensor.sensor_schema(
                    device_class=DEVICE_CLASS_DISTANCE,
                    unit_of_measurement=UNIT_MILLIMETER,
                    icon="mdi:relation-zero-or-one-to-zero-or-one",
                ),
            }
        )
        for n in range(MAX_TARGETS)
    },
    {
        cv.Optional(f"zone_{n+1}"): cv.Schema(
            {
                cv.Optional(CONF_TARGET_COUNT): sensor.sensor_schema(
                    icon="mdi:map-marker-account",
                ),
                cv.Optional(CONF_STILL_TARGET_COUNT): sensor.sensor_schema(
                    icon="mdi:map-marker-account",
                ),
                cv.Optional(CONF_MOVING_TARGET_COUNT): sensor.sensor_schema(
                    icon="mdi:map-marker-account",
                ),
            }
        )
        for n in range(MAX_ZONES)
    },
)


async def to_code(config):
    ld2450_component = await cg.get_variable(config[CONF_LD2450_ID])

    if target_count_config := config.get(CONF_TARGET_COUNT):
        sens = await sensor.new_sensor(target_count_config)
        cg.add(ld2450_component.set_target_count_sensor(sens))

    if still_target_count_config := config.get(CONF_STILL_TARGET_COUNT):
        sens = await sensor.new_sensor(still_target_count_config)
        cg.add(ld2450_component.set_still_target_count_sensor(sens))

    if moving_target_count_config := config.get(CONF_MOVING_TARGET_COUNT):
        sens = await sensor.new_sensor(moving_target_count_config)
        cg.add(ld2450_component.set_moving_target_count_sensor(sens))
    for n in range(MAX_TARGETS):
        if target_conf := config.get(f"target_{n+1}"):
            if x_config := target_conf.get(CONF_X):
                sens = await sensor.new_sensor(x_config)
                cg.add(ld2450_component.set_move_x_sensor(n, sens))
            if y_config := target_conf.get(CONF_Y):
                sens = await sensor.new_sensor(y_config)
                cg.add(ld2450_component.set_move_y_sensor(n, sens))
            if speed_config := target_conf.get(CONF_SPEED):
                sens = await sensor.new_sensor(speed_config)
                cg.add(ld2450_component.set_move_speed_sensor(n, sens))
            if angle_config := target_conf.get(CONF_ANGLE):
                sens = await sensor.new_sensor(angle_config)
                cg.add(ld2450_component.set_move_angle_sensor(n, sens))
            if distance_config := target_conf.get(CONF_DISTANCE):
                sens = await sensor.new_sensor(distance_config)
                cg.add(ld2450_component.set_move_distance_sensor(n, sens))
            if resolution_config := target_conf.get(CONF_RESOLUTION):
                sens = await sensor.new_sensor(resolution_config)
                cg.add(ld2450_component.set_move_resolution_sensor(n, sens))
    for n in range(MAX_ZONES):
        if zone_config := config.get(f"zone_{n+1}"):
            if target_count_config := zone_config.get(CONF_TARGET_COUNT):
                sens = await sensor.new_sensor(target_count_config)
                cg.add(ld2450_component.set_zone_target_count_sensor(n, sens))
            if still_target_count_config := zone_config.get(CONF_STILL_TARGET_COUNT):
                sens = await sensor.new_sensor(still_target_count_config)
                cg.add(ld2450_component.set_zone_still_target_count_sensor(n, sens))
            if moving_target_count_config := zone_config.get(CONF_MOVING_TARGET_COUNT):
                sens = await sensor.new_sensor(moving_target_count_config)
                cg.add(ld2450_component.set_zone_moving_target_count_sensor(n, sens))
