import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_TYPE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
    ICON_LIGHTBULB,
)
from . import APDS9960, CONF_APDS9960_ID

DEPENDENCIES = ["apds9960"]

TYPES = ["clear", "red", "green", "blue", "proximity"]

CONFIG_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_PERCENT,
    icon=ICON_LIGHTBULB,
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
).extend(
    {
        cv.Required(CONF_TYPE): cv.one_of(*TYPES, lower=True),
        cv.GenerateID(CONF_APDS9960_ID): cv.use_id(APDS9960),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_APDS9960_ID])
    var = await sensor.new_sensor(config)
    func = getattr(hub, f"set_{config[CONF_TYPE]}_sensor")
    cg.add(func(var))
