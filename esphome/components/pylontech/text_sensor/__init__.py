import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import CONF_BATTERY, CONF_PYLONTECH_ID, PYLONTECH_COMPONENT_SCHEMA, pylontech_ns

PylontechTextSensor = pylontech_ns.class_("PylontechTextSensor", cg.Component)

CONF_BASE_STATE = "base_state"
CONF_VOLTAGE_STATE = "voltage_state"
CONF_CURRENT_STATE = "current_state"
CONF_TEMPERATURE_STATE = "temperature_state"

MARKERS: list[str] = [
    CONF_BASE_STATE,
    CONF_VOLTAGE_STATE,
    CONF_CURRENT_STATE,
    CONF_TEMPERATURE_STATE,
]

CONFIG_SCHEMA = PYLONTECH_COMPONENT_SCHEMA.extend(
    {cv.GenerateID(): cv.declare_id(PylontechTextSensor)}
).extend({cv.Optional(marker): text_sensor.text_sensor_schema() for marker in MARKERS})


async def to_code(config):
    paren = await cg.get_variable(config[CONF_PYLONTECH_ID])
    bat = cg.new_Pvariable(config[CONF_ID], config[CONF_BATTERY])

    for marker in MARKERS:
        if marker_config := config.get(marker):
            var = await text_sensor.new_text_sensor(marker_config)
            cg.add(getattr(bat, f"set_{marker}_text_sensor")(var))

    cg.add(paren.register_listener(bat))
