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
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_SPEED,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_MEASUREMENT_ANGLE,
    UNIT_KILOMETER_PER_HOUR,
)
from esphome.types import ConfigType

from . import CONF_GEOLOCATION_ID, DATA, DATA_PUBLISHER, GEOLOCATION_CHILD_SCHEMA

CONF_HDOP = "hdop"
CONF_HORIZONTAL_ACCURACY = "horizontal_accuracy"
CONF_VERTICAL_ACCURACY = "vertical_accuracy"

ICON_ALTIMETER = "mdi:altimeter"
ICON_CIRCLE_DOUBLE = "mdi:circle-double"
ICON_COMPASS = "mdi:compass"
ICON_LATITUDE = "mdi:latitude"
ICON_LONGITUDE = "mdi:longitude"
ICON_SATELLITE = "mdi:satellite-variant"
ICON_SPEEDOMETER = "mdi:speedometer"


def _accuracy_schema() -> cv.Schema:
    return sensor.sensor_schema(
        kind=sensor.LENGTH_M,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_DISTANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    )


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
    CONF_HORIZONTAL_ACCURACY: _accuracy_schema(),
    CONF_VERTICAL_ACCURACY: _accuracy_schema(),
}

CONFIG_SCHEMA = cv.typed_schema(
    {key: schema.extend(GEOLOCATION_CHILD_SCHEMA) for key, schema in TYPES.items()}
)


# The GeolocationField of each type and the GeolocationData member that gives its value
VALUES = {
    CONF_LATITUDE: ("LOCATION", "latitude()"),
    CONF_LONGITUDE: ("LOCATION", "longitude()"),
    CONF_SPEED: ("SPEED", "speed_kmh()"),
    CONF_COURSE: ("COURSE", "course()"),
    CONF_ALTITUDE: ("ALTITUDE", "altitude()"),
    CONF_SATELLITES: ("SATELLITES", "satellites"),
    CONF_HDOP: ("HDOP", "hdop()"),
    CONF_HORIZONTAL_ACCURACY: ("HORIZONTAL_ACCURACY", "horizontal_accuracy()"),
    CONF_VERTICAL_ACCURACY: ("VERTICAL_ACCURACY", "vertical_accuracy()"),
}


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    parent = await cg.get_variable(config[CONF_GEOLOCATION_ID])
    field, member = VALUES[config[CONF_TYPE]]
    # NAN until the receiver has sent the value
    value = cg.RawExpression(
        f"{DATA}.has(geolocation::GEOLOCATION_FIELD_{field}) ? {DATA}.{member} : NAN"
    )
    DATA_PUBLISHER.add(parent, parent_ref(var).publish_state(value))
