import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID

from .. import (
    CONF_PYLONTECH_ID,
    PYLONTECH_COMPONENT_SCHEMA,
    CONF_BATTERY,
    CONF_BASE_STATE,
    CONF_VOLTAGE_STATE,
    CONF_CURRENT_STATE,
    CONF_TEMPERATURE_STATE,
    pylontech_ns,
)

PylontechTextSensor = pylontech_ns.class_("PylontechTextSensor", cg.Component)

MARKERS: list[str] = [
    CONF_BASE_STATE,
    CONF_VOLTAGE_STATE,
    CONF_CURRENT_STATE,
    CONF_TEMPERATURE_STATE,
]

CONFIG_SCHEMA = PYLONTECH_COMPONENT_SCHEMA.extend(
    {cv.GenerateID(): cv.declare_id(PylontechTextSensor)}
    | {cv.Optional(marker): text_sensor.text_sensor_schema() for marker in MARKERS}
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_PYLONTECH_ID])
    bat = cg.new_Pvariable(config[CONF_ID], config[CONF_BATTERY])

    for marker in MARKERS:
        if marker in config:
            conf = config[marker]
            var = await text_sensor.new_text_sensor(conf)
            cg.add(getattr(bat, f"set_{marker}")(var))

    cg.add(paren.register_listener(bat))
