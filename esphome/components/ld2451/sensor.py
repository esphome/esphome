import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import CONF_TARGET_COUNT
import esphome.config_validation as cv
from esphome.const import (
    CONF_ANGLE,
    CONF_DISTANCE,
    CONF_SPEED,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_SPEED,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_SIGNAL,
    STATE_CLASS_MEASUREMENT,
    UNIT_KILOMETER_PER_HOUR,
)
from esphome.types import ConfigType

from . import CONF_LD2451_ID, LD2451Component

DEPENDENCIES = ["ld2451"]

CONF_SNR = "snr"

ICON_CAR_MULTIPLE = "mdi:car-multiple"
ICON_FORMAT_TEXT_ROTATION_ANGLE_UP = "mdi:format-text-rotation-angle-up"
ICON_MAP_MARKER_DISTANCE = "mdi:map-marker-distance"
ICON_SPEEDOMETER = "mdi:speedometer"

# Hi-Link documents no limit; the datasheet example has 3 and vendor material claims 5
MAX_TARGETS = 5

_TARGET_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ANGLE): sensor.sensor_schema(
            kind=sensor.ANGLE,
            icon=ICON_FORMAT_TEXT_ROTATION_ANGLE_UP,
            accuracy_decimals=0,
        ),
        cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
            kind=sensor.LENGTH_M,
            device_class=DEVICE_CLASS_DISTANCE,
            icon=ICON_MAP_MARKER_DISTANCE,
            accuracy_decimals=0,
        ),
        # Negative while the target approaches
        cv.Optional(CONF_SPEED): sensor.sensor_schema(
            device_class=DEVICE_CLASS_SPEED,
            state_class=STATE_CLASS_MEASUREMENT,
            unit_of_measurement=UNIT_KILOMETER_PER_HOUR,
            icon=ICON_SPEEDOMETER,
            accuracy_decimals=0,
        ),
        cv.Optional(CONF_SNR): sensor.sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon=ICON_SIGNAL,
            accuracy_decimals=0,
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD2451_ID): cv.use_id(LD2451Component),
        cv.Optional(CONF_TARGET_COUNT): sensor.sensor_schema(
            accuracy_decimals=0,
            icon=ICON_CAR_MULTIPLE,
        ),
        **{
            cv.Optional(f"target_{n}"): _TARGET_SCHEMA
            for n in range(1, MAX_TARGETS + 1)
        },
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2451_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TARGET_COUNT, hub.set_target_count_sensor)

    setters = (
        (CONF_ANGLE, hub.set_target_angle_sensor),
        (CONF_DISTANCE, hub.set_target_distance_sensor),
        (CONF_SPEED, hub.set_target_speed_sensor),
        (CONF_SNR, hub.set_target_snr_sensor),
    )
    for n in range(MAX_TARGETS):
        if (target_conf := config.get(f"target_{n + 1}")) is None:
            continue
        for key, setter in setters:
            if (sens_conf := target_conf.get(key)) is not None:
                cg.add(setter(n, await sensor.new_sensor(sens_conf)))
