import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import CONF_TARGET_COUNT
import esphome.config_validation as cv
from esphome.const import (
    CONF_ANGLE,
    CONF_DISTANCE,
    CONF_ID,
    CONF_X,
    CONF_Y,
    DEVICE_CLASS_DISTANCE,
    UNIT_DEGREES,
    UNIT_METER,
)
from esphome.types import ConfigType

from . import CONF_LD2460_ID, LD2460Component

DEPENDENCIES = ["ld2460"]

ICON_ACCOUNT_GROUP = "mdi:account-group"
ICON_ALPHA_X_BOX_OUTLINE = "mdi:alpha-x-box-outline"
ICON_ALPHA_Y_BOX_OUTLINE = "mdi:alpha-y-box-outline"
ICON_FORMAT_TEXT_ROTATION_ANGLE_UP = "mdi:format-text-rotation-angle-up"
ICON_MAP_MARKER_DISTANCE = "mdi:map-marker-distance"

MAX_TARGETS = 5

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
        cv.GenerateID(CONF_LD2460_ID): cv.use_id(LD2460Component),
        cv.Optional(CONF_TARGET_COUNT): sensor.sensor_schema(
            accuracy_decimals=0,
            icon=ICON_ACCOUNT_GROUP,
        ),
    }
)

CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
    {
        cv.Optional(f"target_{n + 1}"): cv.Schema(
            {
                cv.Optional(CONF_X): sensor.sensor_schema(
                    device_class=DEVICE_CLASS_DISTANCE,
                    icon=ICON_ALPHA_X_BOX_OUTLINE,
                    unit_of_measurement=UNIT_METER,
                    accuracy_decimals=1,
                ),
                cv.Optional(CONF_Y): sensor.sensor_schema(
                    device_class=DEVICE_CLASS_DISTANCE,
                    icon=ICON_ALPHA_Y_BOX_OUTLINE,
                    unit_of_measurement=UNIT_METER,
                    accuracy_decimals=1,
                ),
                cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
                    device_class=DEVICE_CLASS_DISTANCE,
                    icon=ICON_MAP_MARKER_DISTANCE,
                    unit_of_measurement=UNIT_METER,
                    accuracy_decimals=2,
                ),
                cv.Optional(CONF_ANGLE): sensor.sensor_schema(
                    icon=ICON_FORMAT_TEXT_ROTATION_ANGLE_UP,
                    unit_of_measurement=UNIT_DEGREES,
                    accuracy_decimals=1,
                ),
            }
        )
        for n in range(MAX_TARGETS)
    }
)


async def to_code(config: ConfigType) -> None:
    ld2460_component = await cg.get_variable(config[CONF_LD2460_ID])

    if target_count_config := config.get(CONF_TARGET_COUNT):
        sens = await sensor.new_sensor(target_count_config)
        cg.add(ld2460_component.set_target_count_sensor(sens))

    for n in range(MAX_TARGETS):
        if target_conf := config.get(f"target_{n + 1}"):
            if x_config := target_conf.get(CONF_X):
                sens = await sensor.new_sensor(x_config)
                cg.add(ld2460_component.set_target_x_sensor(n, sens))
            if y_config := target_conf.get(CONF_Y):
                sens = await sensor.new_sensor(y_config)
                cg.add(ld2460_component.set_target_y_sensor(n, sens))
            if distance_config := target_conf.get(CONF_DISTANCE):
                sens = await sensor.new_sensor(distance_config)
                cg.add(ld2460_component.set_target_distance_sensor(n, sens))
            if angle_config := target_conf.get(CONF_ANGLE):
                sens = await sensor.new_sensor(angle_config)
                cg.add(ld2460_component.set_target_angle_sensor(n, sens))
