from esphome.components import text_sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_ICON,
    ICON_WEATHER_SUNSET_DOWN,
    ICON_WEATHER_SUNSET_UP,
    CONF_TYPE,
    CONF_FORMAT,
)
from .. import sun_ns, CONF_SUN_ID, Sun, CONF_ELEVATION, elevation, DEFAULT_ELEVATION

DEPENDENCIES = ["sun"]

SunTextSensor = sun_ns.class_(
    "SunTextSensor", text_sensor.TextSensor, cg.PollingComponent
)
SUN_TYPES = {
    "sunset": False,
    "sunrise": True,
}


def validate_optional_icon(config):
    if CONF_ICON not in config:
        config = config.copy()
        config[CONF_ICON] = {
            "sunset": ICON_WEATHER_SUNSET_DOWN,
            "sunrise": ICON_WEATHER_SUNSET_UP,
        }[config[CONF_TYPE]]
    return config


CONFIG_SCHEMA = cv.All(
    text_sensor.text_sensor_schema()
    .extend(
        {
            cv.GenerateID(): cv.declare_id(SunTextSensor),
            cv.GenerateID(CONF_SUN_ID): cv.use_id(Sun),
            cv.Required(CONF_TYPE): cv.one_of(*SUN_TYPES, lower=True),
            cv.Optional(CONF_ELEVATION, default=DEFAULT_ELEVATION): elevation,
            cv.Optional(CONF_FORMAT, default="%X"): cv.string_strict,
        }
    )
    .extend(cv.polling_component_schema("60s")),
    validate_optional_icon,
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)

    paren = await cg.get_variable(config[CONF_SUN_ID])
    cg.add(var.set_parent(paren))
    cg.add(var.set_sunrise(SUN_TYPES[config[CONF_TYPE]]))
    cg.add(var.set_elevation(config[CONF_ELEVATION]))
    cg.add(var.set_format(config[CONF_FORMAT]))
