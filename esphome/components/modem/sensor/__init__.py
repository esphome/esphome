import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY,
    CONF_ALTITUDE,
    CONF_COURSE,
    CONF_ID,
    CONF_LATITUDE,
    CONF_LONGITUDE,
    CONF_SPEED,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL,
    UNIT_DEGREES,
    UNIT_KILOMETER_PER_HOUR,
    UNIT_METER,
    UNIT_PERCENT,
)

from .. import MODEM_COMPONENT_SCHEMA, final_validate_platform, modem_ns
from ..switch import GNSS_SWITCH_SCHEMA

CODEOWNERS = ["@oarcher"]

AUTO_LOAD = []

DEPENDENCIES = ["modem"]

IS_PLATFORM_COMPONENT = True

CONF_BER = "ber"
CONF_RSSI = "rssi"

ICON_LATITUDE = "mdi:latitude"
ICON_LONGITUDE = "mdi:longitude"
ICON_LOCATION = "mdi:map-marker"
ICON_COMPASS = "mdi:compass"
ICON_LOCATION_RADIUS = "mdi:map-marker-radius"
ICON_LOCATION_UP = "mdi:map-marker-up"
ICON_SPEED = "mdi:speedometer"
ICON_SIGNAL_BAR = "mdi:signal"

ModemSensor = modem_ns.class_("ModemSensor", cg.PollingComponent)


GNSS_SENSORS = {
    CONF_LATITUDE,
    CONF_LONGITUDE,
    CONF_ALTITUDE,
    CONF_COURSE,
    CONF_ACCURACY,
    CONF_SPEED,
}

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModemSensor),
            cv.Optional(CONF_RSSI): sensor.sensor_schema(
                unit_of_measurement=UNIT_DECIBEL,
                accuracy_decimals=0,
                icon=ICON_SIGNAL_BAR,
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_BER): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_LATITUDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                accuracy_decimals=5,
                icon=ICON_LATITUDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(GNSS_SWITCH_SCHEMA),
            cv.Optional(CONF_LONGITUDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                accuracy_decimals=5,
                icon=ICON_LONGITUDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(GNSS_SWITCH_SCHEMA),
            cv.Optional(CONF_ALTITUDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_METER,
                accuracy_decimals=1,
                icon=ICON_LOCATION_UP,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(GNSS_SWITCH_SCHEMA),
            cv.Optional(CONF_SPEED): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOMETER_PER_HOUR,
                accuracy_decimals=1,
                icon=ICON_SPEED,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(GNSS_SWITCH_SCHEMA),
            cv.Optional(CONF_ACCURACY): sensor.sensor_schema(
                unit_of_measurement=UNIT_METER,
                accuracy_decimals=1,
                icon=ICON_LOCATION_RADIUS,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(GNSS_SWITCH_SCHEMA),
            cv.Optional(CONF_COURSE): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                accuracy_decimals=1,
                icon=ICON_COMPASS,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(GNSS_SWITCH_SCHEMA),
        }
    )
    .extend(MODEM_COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("60s")),
)

FINAL_VALIDATE_SCHEMA = cv.All(final_validate_platform)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    if rssi := config.get(CONF_RSSI, None):
        rssi_sensor = await sensor.new_sensor(rssi)
        cg.add(var.set_rssi_sensor(rssi_sensor))

    if ber := config.get(CONF_BER, None):
        ber_sensor = await sensor.new_sensor(ber)
        cg.add(var.set_ber_sensor(ber_sensor))

    if latitude := config.get(CONF_LATITUDE, None):
        latitude_sensor = await sensor.new_sensor(latitude)
        cg.add(var.set_latitude_sensor(latitude_sensor))

    if longitude := config.get(CONF_LONGITUDE, None):
        longitude_sensor = await sensor.new_sensor(longitude)
        cg.add(var.set_longitude_sensor(longitude_sensor))

    if altitude := config.get(CONF_ALTITUDE, None):
        altitude_sensor = await sensor.new_sensor(altitude)
        cg.add(var.set_altitude_sensor(altitude_sensor))

    if speed := config.get(CONF_SPEED, None):
        speed_sensor = await sensor.new_sensor(speed)
        cg.add(var.set_speed_sensor(speed_sensor))

    if course := config.get(CONF_COURSE, None):
        course_sensor = await sensor.new_sensor(course)
        cg.add(var.set_course_sensor(course_sensor))

    if accuracy := config.get(CONF_ACCURACY, None):
        accuracy_sensor = await sensor.new_sensor(accuracy)
        cg.add(var.set_accuracy_sensor(accuracy_sensor))

    await cg.register_component(var, config)
