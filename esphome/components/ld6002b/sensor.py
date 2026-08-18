import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import CONF_TARGET_COUNT
import esphome.config_validation as cv
from esphome.const import (
    CONF_X,
    CONF_Y,
    DEVICE_CLASS_DISTANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
)

from . import LD6002BComponent
from .const import (
    AREA_COUNT,
    CONF_CLUSTER_ID,
    CONF_DOPPLER_INDEX,
    CONF_LD6002B_ID,
    CONF_POINT_COUNT,
    CONF_Z,
    CONF_Z_MAX,
    CONF_Z_MIN,
    KEY_X_MAX,
    KEY_X_MIN,
    KEY_Y_MAX,
    KEY_Y_MIN,
    MAX_TARGETS,
)

DEPENDENCIES = ["ld6002b"]

# The ld2450 defaults for a streamed value: hold the last reading for a second so a
# dropped frame does not read as absence, then rate-limit what reaches the frontend.
_VALUE_SENSOR_FILTERS = [
    {
        "timeout": {
            "timeout": cv.TimePeriod(milliseconds=1000),
            "value": "last",
        }
    },
    {"throttle_with_priority": cv.TimePeriod(milliseconds=1000)},
]

TARGET_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_X): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            filters=_VALUE_SENSOR_FILTERS,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_Y): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            filters=_VALUE_SENSOR_FILTERS,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_Z): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            filters=_VALUE_SENSOR_FILTERS,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_DOPPLER_INDEX): sensor.sensor_schema(
            accuracy_decimals=0,
            filters=_VALUE_SENSOR_FILTERS,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CLUSTER_ID): sensor.sensor_schema(
            accuracy_decimals=0,
        ),
    }
)

AREA_SCHEMA = cv.Schema(
    {
        cv.Optional(KEY_X_MIN): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(KEY_X_MAX): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(KEY_Y_MIN): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(KEY_Y_MAX): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_Z_MIN): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_Z_MAX): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)

# (config key, C++ setter axis) for the six bounds every area sensor block carries.
_AREA_AXES = (
    (KEY_X_MIN, "x_min"),
    (KEY_X_MAX, "x_max"),
    (KEY_Y_MIN, "y_min"),
    (KEY_Y_MAX, "y_max"),
    (CONF_Z_MIN, "z_min"),
    (CONF_Z_MAX, "z_max"),
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_LD6002B_ID): cv.use_id(LD6002BComponent),
            cv.Optional(CONF_TARGET_COUNT): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POINT_COUNT): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend({cv.Optional(f"target_{i + 1}"): TARGET_SCHEMA for i in range(MAX_TARGETS)})
    .extend(
        {cv.Optional(f"interference_area_{i}"): AREA_SCHEMA for i in range(AREA_COUNT)}
    )
    .extend(
        {cv.Optional(f"detection_area_{i}"): AREA_SCHEMA for i in range(AREA_COUNT)}
    )
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_LD6002B_ID])

    if target_count_config := config.get(CONF_TARGET_COUNT):
        sens = await sensor.new_sensor(target_count_config)
        cg.add(hub.set_target_count_sensor(sens))

    if point_count_config := config.get(CONF_POINT_COUNT):
        sens = await sensor.new_sensor(point_count_config)
        cg.add(hub.set_point_count_sensor(sens))

    for i in range(MAX_TARGETS):
        if target_config := config.get(f"target_{i + 1}"):
            if x_config := target_config.get(CONF_X):
                sens = await sensor.new_sensor(x_config)
                cg.add(hub.set_target_x_sensor(i, sens))
            if y_config := target_config.get(CONF_Y):
                sens = await sensor.new_sensor(y_config)
                cg.add(hub.set_target_y_sensor(i, sens))
            if z_config := target_config.get(CONF_Z):
                sens = await sensor.new_sensor(z_config)
                cg.add(hub.set_target_z_sensor(i, sens))
            if doppler_index_config := target_config.get(CONF_DOPPLER_INDEX):
                sens = await sensor.new_sensor(doppler_index_config)
                cg.add(hub.set_target_dop_idx_sensor(i, sens))
            if cluster_id_config := target_config.get(CONF_CLUSTER_ID):
                sens = await sensor.new_sensor(cluster_id_config)
                cg.add(hub.set_target_cluster_id_sensor(i, sens))

    for kind in ("interference", "detection"):
        for i in range(AREA_COUNT):
            if area_config := config.get(f"{kind}_area_{i}"):
                for key, axis in _AREA_AXES:
                    if axis_config := area_config.get(key):
                        sens = await sensor.new_sensor(axis_config)
                        cg.add(getattr(hub, f"set_{kind}_area_{axis}_sensor")(i, sens))
