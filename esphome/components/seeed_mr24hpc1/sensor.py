import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_SPEED,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
)
from esphome.types import ConfigType

from . import CONF_MR24HPC1_ID, MR24HPC1Component

CONF_CUSTOM_PRESENCE_OF_DETECTION = "custom_presence_of_detection"
CONF_MOVEMENT_SIGNS = "movement_signs"
CONF_CUSTOM_MOTION_DISTANCE = "custom_motion_distance"
CONF_CUSTOM_SPATIAL_STATIC_VALUE = "custom_spatial_static_value"
CONF_CUSTOM_SPATIAL_MOTION_VALUE = "custom_spatial_motion_value"
CONF_CUSTOM_MOTION_SPEED = "custom_motion_speed"
CONF_CUSTOM_MODE_NUM = "custom_mode_num"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MR24HPC1_ID): cv.use_id(MR24HPC1Component),
        cv.Optional(CONF_CUSTOM_PRESENCE_OF_DETECTION): sensor.sensor_schema(
            device_class=DEVICE_CLASS_DISTANCE,
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,  # Specify the number of decimal places
            icon="mdi:signal-distance-variant",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_MOVEMENT_SIGNS): sensor.sensor_schema(
            icon="mdi:human-greeting-variant",
        ),
        cv.Optional(CONF_CUSTOM_MOTION_DISTANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            icon="mdi:signal-distance-variant",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CUSTOM_SPATIAL_STATIC_VALUE): sensor.sensor_schema(
            device_class=DEVICE_CLASS_ENERGY,
            icon="mdi:counter",
        ),
        cv.Optional(CONF_CUSTOM_SPATIAL_MOTION_VALUE): sensor.sensor_schema(
            device_class=DEVICE_CLASS_ENERGY,
            icon="mdi:counter",
        ),
        cv.Optional(CONF_CUSTOM_MOTION_SPEED): sensor.sensor_schema(
            unit_of_measurement="m/s",
            device_class=DEVICE_CLASS_SPEED,
            accuracy_decimals=2,
            icon="mdi:run-fast",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CUSTOM_MODE_NUM): sensor.sensor_schema(
            icon="mdi:counter",
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MR24HPC1_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(
        CONF_CUSTOM_PRESENCE_OF_DETECTION, hub.set_custom_presence_of_detection_sensor
    )
    await sensors(CONF_MOVEMENT_SIGNS, hub.set_movement_signs_sensor)
    await sensors(CONF_CUSTOM_MOTION_DISTANCE, hub.set_custom_motion_distance_sensor)
    await sensors(
        CONF_CUSTOM_SPATIAL_STATIC_VALUE, hub.set_custom_spatial_static_value_sensor
    )
    await sensors(
        CONF_CUSTOM_SPATIAL_MOTION_VALUE, hub.set_custom_spatial_motion_value_sensor
    )
    await sensors(CONF_CUSTOM_MOTION_SPEED, hub.set_custom_motion_speed_sensor)
    await sensors(CONF_CUSTOM_MODE_NUM, hub.set_custom_mode_num_sensor)
