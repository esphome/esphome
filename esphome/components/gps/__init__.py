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
    STATE_CLASS_MEASUREMENT_ANGLE,
    UNIT_DEGREES,
    UNIT_KILOMETER_PER_HOUR,
    UNIT_METER,
)

CONF_GPS_ID = "gps_id"
CONF_HDOP = "hdop"

ICON_ALTIMETER = "mdi:altimeter"
ICON_COMPASS = "mdi:compass"
ICON_CIRCLE_DOUBLE = "mdi:circle-double"
ICON_LATITUDE = "mdi:latitude"
ICON_LONGITUDE = "mdi:longitude"
ICON_SATELLITE = "mdi:satellite-variant"
ICON_SPEEDOMETER = "mdi:speedometer"

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor"]

CODEOWNERS = ["@coogle", "@ximex"]

gps_ns = cg.esphome_ns.namespace("gps")
GPS = gps_ns.class_("GPS", cg.PollingComponent, uart.UARTDevice)
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
                state_class=STATE_CLASS_MEASUREMENT_ANGLE,
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
                state_class=STATE_CLASS_MEASUREMENT_ANGLE,
            ),
            cv.Optional(CONF_ALTITUDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_METER,
                icon=ICON_ALTIMETER,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SATELLITES): sensor.sensor_schema(
                # no unit_of_measurement
                icon=ICON_SATELLITE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HDOP): sensor.sensor_schema(
                # no unit_of_measurement
                icon=ICON_CIRCLE_DOUBLE,
                accuracy_decimals=3,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("20s"))
    .extend(uart.UART_DEVICE_SCHEMA),
)
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema("gps", require_rx=True)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    # Pre-create all sensor variables so automations that reference
    # sibling sensors don't deadlock waiting for unregistered IDs.
    sensors = [
        (cg.new_Pvariable(conf[CONF_ID]), conf, setter)
        for key, setter in (
            (CONF_LATITUDE, "set_latitude_sensor"),
            (CONF_LONGITUDE, "set_longitude_sensor"),
            (CONF_SPEED, "set_speed_sensor"),
            (CONF_COURSE, "set_course_sensor"),
            (CONF_ALTITUDE, "set_altitude_sensor"),
            (CONF_SATELLITES, "set_satellites_sensor"),
            (CONF_HDOP, "set_hdop_sensor"),
        )
        if (conf := config.get(key))
    ]

    for sens, conf, setter in sensors:
        await sensor.register_sensor(sens, conf)
        cg.add(getattr(var, setter)(sens))

    # https://platformio.org/lib/show/1655/TinyGPSPlus
    # Using fork of TinyGPSPlus patched to build on ESP-IDF
    cg.add_library(
        "TinyGPSPlus",
        None,
        "https://github.com/esphome/TinyGPSPlus.git#v1.1.0",
    )
