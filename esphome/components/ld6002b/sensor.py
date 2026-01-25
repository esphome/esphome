import esphome.codegen as cg
from esphome.components import sensor
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
    CONF_CLUSTER_ID,
    CONF_DOP_IDX,
    CONF_LD6002B_ID,
    CONF_POINT_COUNT,
    CONF_Z,
    CONF_Z_MAX,
    CONF_Z_MIN,
    KEY_TARGET_COUNT,
    KEY_X_MAX,
    KEY_X_MIN,
    KEY_Y_MAX,
    KEY_Y_MIN,
)

DEPENDENCIES = ["ld6002b"]

MAX_TARGETS = 3
AREA_COUNT = 4

TARGET_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_X): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_Y): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_Z): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_DOP_IDX): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CLUSTER_ID): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
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

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_LD6002B_ID): cv.use_id(LD6002BComponent),
            cv.Optional(KEY_TARGET_COUNT): sensor.sensor_schema(
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

    if target_count_config := config.get(KEY_TARGET_COUNT):
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
            if dop_idx_config := target_config.get(CONF_DOP_IDX):
                sens = await sensor.new_sensor(dop_idx_config)
                cg.add(hub.set_target_dop_idx_sensor(i, sens))
            if cluster_id_config := target_config.get(CONF_CLUSTER_ID):
                sens = await sensor.new_sensor(cluster_id_config)
                cg.add(hub.set_target_cluster_id_sensor(i, sens))

    for i in range(AREA_COUNT):
        if area_config := config.get(f"interference_area_{i}"):
            if x_min_config := area_config.get(KEY_X_MIN):
                sens = await sensor.new_sensor(x_min_config)
                cg.add(hub.set_interference_area_x_min_sensor(i, sens))
            if x_max_config := area_config.get(KEY_X_MAX):
                sens = await sensor.new_sensor(x_max_config)
                cg.add(hub.set_interference_area_x_max_sensor(i, sens))
            if y_min_config := area_config.get(KEY_Y_MIN):
                sens = await sensor.new_sensor(y_min_config)
                cg.add(hub.set_interference_area_y_min_sensor(i, sens))
            if y_max_config := area_config.get(KEY_Y_MAX):
                sens = await sensor.new_sensor(y_max_config)
                cg.add(hub.set_interference_area_y_max_sensor(i, sens))
            if z_min_config := area_config.get(CONF_Z_MIN):
                sens = await sensor.new_sensor(z_min_config)
                cg.add(hub.set_interference_area_z_min_sensor(i, sens))
            if z_max_config := area_config.get(CONF_Z_MAX):
                sens = await sensor.new_sensor(z_max_config)
                cg.add(hub.set_interference_area_z_max_sensor(i, sens))

    for i in range(AREA_COUNT):
        if area_config := config.get(f"detection_area_{i}"):
            if x_min_config := area_config.get(KEY_X_MIN):
                sens = await sensor.new_sensor(x_min_config)
                cg.add(hub.set_detection_area_x_min_sensor(i, sens))
            if x_max_config := area_config.get(KEY_X_MAX):
                sens = await sensor.new_sensor(x_max_config)
                cg.add(hub.set_detection_area_x_max_sensor(i, sens))
            if y_min_config := area_config.get(KEY_Y_MIN):
                sens = await sensor.new_sensor(y_min_config)
                cg.add(hub.set_detection_area_y_min_sensor(i, sens))
            if y_max_config := area_config.get(KEY_Y_MAX):
                sens = await sensor.new_sensor(y_max_config)
                cg.add(hub.set_detection_area_y_max_sensor(i, sens))
            if z_min_config := area_config.get(CONF_Z_MIN):
                sens = await sensor.new_sensor(z_min_config)
                cg.add(hub.set_detection_area_z_min_sensor(i, sens))
            if z_max_config := area_config.get(CONF_Z_MAX):
                sens = await sensor.new_sensor(z_max_config)
                cg.add(hub.set_detection_area_z_max_sensor(i, sens))
