import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE, ICON_WEATHER_SUNSET, UNIT_DEGREES

from .. import CONF_SUN_ID, Sun, sun_ns

DEPENDENCIES = ["sun"]

SunSensor = sun_ns.class_("SunSensor", sensor.Sensor, cg.PollingComponent)
SensorType = sun_ns.enum("SensorType")
TYPES = {
    "elevation": SensorType.SUN_SENSOR_ELEVATION,
    "azimuth": SensorType.SUN_SENSOR_AZIMUTH,
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        SunSensor,
        unit_of_measurement=UNIT_DEGREES,
        icon=ICON_WEATHER_SUNSET,
        accuracy_decimals=1,
    )
    .extend(
        {
            cv.GenerateID(CONF_SUN_ID): cv.use_id(Sun),
            cv.Required(CONF_TYPE): cv.enum(TYPES, lower=True),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    cg.add(var.set_type(config[CONF_TYPE]))
    paren = await cg.get_variable(config[CONF_SUN_ID])
    cg.add(var.set_parent(paren))
