import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import CONF_TARGET_COUNT
import esphome.config_validation as cv
from esphome.const import (
    CONF_ANGLE,
    CONF_DISTANCE,
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

_TARGET_SCHEMA = cv.Schema(
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

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD2460_ID): cv.use_id(LD2460Component),
        cv.Optional(CONF_TARGET_COUNT): sensor.sensor_schema(
            accuracy_decimals=0,
            icon=ICON_ACCOUNT_GROUP,
        ),
        **{
            cv.Optional(f"target_{n}"): _TARGET_SCHEMA
            for n in range(1, MAX_TARGETS + 1)
        },
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2460_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TARGET_COUNT, hub.set_target_count_sensor)

    setters = (
        (CONF_X, hub.set_target_x_sensor),
        (CONF_Y, hub.set_target_y_sensor),
        (CONF_DISTANCE, hub.set_target_distance_sensor),
        (CONF_ANGLE, hub.set_target_angle_sensor),
    )
    for n in range(MAX_TARGETS):
        if (target_conf := config.get(f"target_{n + 1}")) is None:
            continue
        for key, setter in setters:
            if (sens_conf := target_conf.get(key)) is not None:
                cg.add(setter(n, await sensor.new_sensor(sens_conf)))
