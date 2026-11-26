from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ALTITUDE,
    CONF_COURSE,
    CONF_LATITUDE,
    CONF_LONGITUDE,
    CONF_SATELLITES,
    CONF_SPEED,
    CONF_TYPE,
    DEVICE_CLASS_SPEED,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_MEASUREMENT_ANGLE,
    UNIT_KILOMETER_PER_HOUR,
)
from esphome.types import ConfigType

from . import CONF_GNSS_ID, DATA, DATA_PUBLISHER, GNSS_CHILD_SCHEMA

CONF_HDOP = "hdop"

ICON_ALTIMETER = "mdi:altimeter"
ICON_CIRCLE_DOUBLE = "mdi:circle-double"
ICON_COMPASS = "mdi:compass"
ICON_LATITUDE = "mdi:latitude"
ICON_LONGITUDE = "mdi:longitude"
ICON_SATELLITE = "mdi:satellite-variant"
ICON_SPEEDOMETER = "mdi:speedometer"

TYPES = {
    CONF_LATITUDE: sensor.sensor_schema(
        kind=sensor.ANGLE,
        icon=ICON_LATITUDE,
        accuracy_decimals=6,
    ),
    CONF_LONGITUDE: sensor.sensor_schema(
        kind=sensor.ANGLE,
        icon=ICON_LONGITUDE,
        accuracy_decimals=6,
        state_class=STATE_CLASS_MEASUREMENT_ANGLE,
    ),
    CONF_SPEED: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOMETER_PER_HOUR,
        icon=ICON_SPEEDOMETER,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_SPEED,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_COURSE: sensor.sensor_schema(
        kind=sensor.ANGLE,
        icon=ICON_COMPASS,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT_ANGLE,
    ),
    CONF_ALTITUDE: sensor.sensor_schema(kind=sensor.LENGTH_M, icon=ICON_ALTIMETER),
    CONF_SATELLITES: sensor.sensor_schema(
        icon=ICON_SATELLITE,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_HDOP: sensor.sensor_schema(
        icon=ICON_CIRCLE_DOUBLE,
        accuracy_decimals=3,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}

CONFIG_SCHEMA = cv.typed_schema(
    {key: schema.extend(GNSS_CHILD_SCHEMA) for key, schema in TYPES.items()}
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    parent = await cg.get_variable(config[CONF_GNSS_ID])
    DATA_PUBLISHER.add(
        parent, parent_ref(var).publish_state(getattr(DATA, config[CONF_TYPE]))
    )
