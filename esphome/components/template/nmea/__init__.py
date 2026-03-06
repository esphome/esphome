import esphome.codegen as cg
from esphome.components import nmea, sensor
import esphome.config_validation as cv

from .. import template_ns

CONF_LATITUDE = "latitude"
CONF_LONGITUDE = "longitude"
CONF_ALTITUDE = "altitude"
CONF_SPEED = "speed"
CONF_COURSE = "course"
CONF_HDOP = "hdop"
CONF_SATELLITES = "satellites"

DEPENDENCIES = ["nmea"]
CODEOWNERS = ["@oarcher"]

TemplateNMEA = template_ns.class_("TemplateNMEA", nmea.NMEAComponent)

CONFIG_SCHEMA = cv.All(
    nmea.NMEA_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(TemplateNMEA),
            cv.Required(CONF_LATITUDE): cv.use_id(sensor.Sensor),
            cv.Required(CONF_LONGITUDE): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_ALTITUDE): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_SPEED): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_COURSE): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_HDOP): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_SATELLITES): cv.use_id(sensor.Sensor),
        }
    )
)


async def to_code(config):
    var = await nmea.new_nmea(config)

    # Required sensors
    lat = await cg.get_variable(config[CONF_LATITUDE])
    cg.add(var.set_latitude_sensor(lat))

    lon = await cg.get_variable(config[CONF_LONGITUDE])
    cg.add(var.set_longitude_sensor(lon))

    # Optional sensors
    if CONF_ALTITUDE in config:
        alt = await cg.get_variable(config[CONF_ALTITUDE])
        cg.add(var.set_altitude_sensor(alt))

    if CONF_SPEED in config:
        spd = await cg.get_variable(config[CONF_SPEED])
        cg.add(var.set_speed_sensor(spd))

    if CONF_COURSE in config:
        crs = await cg.get_variable(config[CONF_COURSE])
        cg.add(var.set_course_sensor(crs))

    if CONF_HDOP in config:
        hdop = await cg.get_variable(config[CONF_HDOP])
        cg.add(var.set_hdop_sensor(hdop))

    if CONF_SATELLITES in config:
        sats = await cg.get_variable(config[CONF_SATELLITES])
        cg.add(var.set_satellites_sensor(sats))
