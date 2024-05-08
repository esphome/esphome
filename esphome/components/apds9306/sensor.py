import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_TYPE,
    STATE_CLASS_MEASUREMENT,
    ICON_LIGHTBULB
)
from . import APDS9306, CONF_APDS9306_ID

DEPENDENCIES = ["apds9306"]

TYPES = ["light_level"]

CONFIG_SCHEMA = sensor.sensor_schema(
    icon = ICON_LIGHTBULB,
    accuracy_decimals = 0,
    state_class = STATE_CLASS_MEASUREMENT
).extend(
    {   
        cv.Required(CONF_TYPE): cv.one_of(*TYPES, lower=True),
        cv.GenerateID(CONF_APDS9306_ID): cv.use_id(APDS9306)
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_APDS9306_ID])
    var = await sensor.new_sensor(config)
    func = getattr(hub, f"set_{config[CONF_TYPE]}_sensor")
    cg.add(func(var))
