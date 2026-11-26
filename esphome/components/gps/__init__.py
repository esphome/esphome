import logging

import esphome.codegen as cg
from esphome.components import sensor, uart
from esphome.components.gnss.sensor import CONF_HDOP, TYPES
import esphome.config_validation as cv
from esphome.const import (
    CONF_ALTITUDE,
    CONF_COURSE,
    CONF_ID,
    CONF_LATITUDE,
    CONF_LONGITUDE,
    CONF_SATELLITES,
    CONF_SPEED,
)
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CONF_GPS_ID = "gps_id"


DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor"]

CODEOWNERS = ["@coogle", "@ximex"]
DOMAIN = "gps"

DEPRECATED_COMPONENT = """
The 'gps' component is deprecated in favour of 'gnss' with 'platform: nmea'.
New features and fixes should target 'gnss'. It will be removed in ESPHome 2027.6.0.
"""

gps_ns = cg.esphome_ns.namespace("gps")
GPS = gps_ns.class_("GPS", cg.PollingComponent, uart.UARTDevice)
GPSListener = gps_ns.class_("GPSListener")

MULTI_CONF = True

_register_listener = cg.slot_list("GPS_LISTENER_COUNT", "register_listener", DOMAIN)


async def register_listener(hub: MockObj, var: MockObj) -> None:
    """Register a listener with its GPS."""
    cg.add(_register_listener(hub, var))


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GPS),
            **{cv.Optional(key): schema for key, schema in TYPES.items()},
        }
    )
    .extend(cv.polling_component_schema("20s"))
    .extend(uart.UART_DEVICE_SCHEMA),
)
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema("gps", require_rx=True)


async def to_code(config: ConfigType) -> None:
    # Remove before 2027.6.0
    _LOGGER.warning(
        "The 'gps' component is deprecated and will be removed in ESPHome 2027.6.0, migrate to a "
        "'gnss:' entry with 'platform: nmea', 'sensor:' entries with 'platform: gnss' and a 'type:', "
        "and 'time:' with 'platform: gnss'."
    )
    var = await uart.new_uart_component(config)

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
