import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_LATITUDE,
    CONF_LONGITUDE,
    CONF_SPEED,
    CONF_COURSE,
    CONF_ALTITUDE,
    CONF_SATELLITES,
    STATE_CLASS_MEASUREMENT,
    UNIT_DEGREES,
    UNIT_KILOMETER_PER_HOUR,
    UNIT_METER,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor"]

CODEOWNERS = ["@coogle"]

gps_ns = cg.esphome_ns.namespace("gps")
GPS = gps_ns.class_("GPS", cg.Component, uart.UARTDevice)
GPSListener = gps_ns.class_("GPSListener")

CONF_GPS_ID = "gps_id"
MULTI_CONF = True
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GPS),
            cv.Optional(CONF_LATITUDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                accuracy_decimals=6,
            ),
            cv.Optional(CONF_LONGITUDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                accuracy_decimals=6,
            ),
            cv.Optional(CONF_SPEED): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOMETER_PER_HOUR,
                accuracy_decimals=6,
            ),
            cv.Optional(CONF_COURSE): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                accuracy_decimals=2,
            ),
            cv.Optional(CONF_ALTITUDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_METER,
                accuracy_decimals=1,
            ),
            cv.Optional(CONF_SATELLITES): sensor.sensor_schema(
                accuracy_decimals=0,
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

    if CONF_LATITUDE in config:
        sens = await sensor.new_sensor(config[CONF_LATITUDE])
        cg.add(var.set_latitude_sensor(sens))

    if CONF_LONGITUDE in config:
        sens = await sensor.new_sensor(config[CONF_LONGITUDE])
        cg.add(var.set_longitude_sensor(sens))

    if CONF_SPEED in config:
        sens = await sensor.new_sensor(config[CONF_SPEED])
        cg.add(var.set_speed_sensor(sens))

    if CONF_COURSE in config:
        sens = await sensor.new_sensor(config[CONF_COURSE])
        cg.add(var.set_course_sensor(sens))

    if CONF_ALTITUDE in config:
        sens = await sensor.new_sensor(config[CONF_ALTITUDE])
        cg.add(var.set_altitude_sensor(sens))

    if CONF_SATELLITES in config:
        sens = await sensor.new_sensor(config[CONF_SATELLITES])
        cg.add(var.set_satellites_sensor(sens))

    # https://platformio.org/lib/show/1655/TinyGPSPlus
    cg.add_library("mikalhart/TinyGPSPlus", "1.0.2")
