import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ALTITUDE,
    CONF_COURSE,
    CONF_ID,
    CONF_LATITUDE,
    CONF_LONGITUDE,
    CONF_SATELLITES,
    CONF_SPEED,
    DEVICE_CLASS_SPEED,
    STATE_CLASS_MEASUREMENT,
    UNIT_DEGREES,
    UNIT_KILOMETER_PER_HOUR,
    UNIT_METER,
)

CONF_GPS_ID = "gps_id"
CONF_HDOP = "hdop"

ICON_ALTIMETER = "mdi:altimeter"
ICON_COMPASS = "mdi:compass"
ICON_LATITUDE = "mdi:latitude"
ICON_LONGITUDE = "mdi:longitude"
ICON_SATELLITE = "mdi:satellite-variant"
ICON_SPEEDOMETER = "mdi:speedometer"

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor"]

CODEOWNERS = ["@coogle", "@ximex"]

gps_ns = cg.esphome_ns.namespace("gps")
GPS = gps_ns.class_("GPS", cg.Component, uart.UARTDevice)
GPSListener = gps_ns.class_("GPSListener")

MULTI_CONF = True
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GPS),
            cv.Optional(CONF_LATITUDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                icon=ICON_LATITUDE,
                accuracy_decimals=6,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_LONGITUDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                icon=ICON_LONGITUDE,
                accuracy_decimals=6,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SPEED): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOMETER_PER_HOUR,
                icon=ICON_SPEEDOMETER,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_SPEED,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_COURSE): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                icon=ICON_COMPASS,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ALTITUDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_METER,
                icon=ICON_ALTIMETER,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SATELLITES): sensor.sensor_schema(
                icon=ICON_SATELLITE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HDOP): sensor.sensor_schema(
                accuracy_decimals=3,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("20s"))
    .extend(uart.UART_DEVICE_SCHEMA),
    cv.only_with_arduino,
)
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema("gps", require_rx=True)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if latitude_config := config.get(CONF_LATITUDE):
        sens = await sensor.new_sensor(latitude_config)
        cg.add(var.set_latitude_sensor(sens))

    if longitude_config := config.get(CONF_LONGITUDE):
        sens = await sensor.new_sensor(longitude_config)
        cg.add(var.set_longitude_sensor(sens))

    if speed_config := config.get(CONF_SPEED):
        sens = await sensor.new_sensor(speed_config)
        cg.add(var.set_speed_sensor(sens))

    if course_config := config.get(CONF_COURSE):
        sens = await sensor.new_sensor(course_config)
        cg.add(var.set_course_sensor(sens))

    if altitude_config := config.get(CONF_ALTITUDE):
        sens = await sensor.new_sensor(altitude_config)
        cg.add(var.set_altitude_sensor(sens))

    if satellites_config := config.get(CONF_SATELLITES):
        sens = await sensor.new_sensor(satellites_config)
        cg.add(var.set_satellites_sensor(sens))

    if hdop_config := config.get(CONF_HDOP):
        sens = await sensor.new_sensor(hdop_config)
        cg.add(var.set_hdop_sensor(sens))

    # https://platformio.org/lib/show/1655/TinyGPSPlus
    cg.add_library("mikalhart/TinyGPSPlus", "1.1.0")
