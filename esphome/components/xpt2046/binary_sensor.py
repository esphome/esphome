import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from . import (
    xpt2046_ns,
    XPT2046Component,
    CONF_XPT2046_ID,
)

CONF_X_MIN = "x_min"
CONF_X_MAX = "x_max"
CONF_Y_MIN = "y_min"
CONF_Y_MAX = "y_max"

DEPENDENCIES = ["xpt2046"]
XPT2046Button = xpt2046_ns.class_("XPT2046Button", binary_sensor.BinarySensor)


def validate_xpt2046_button(config):
    if cv.int_(config[CONF_X_MAX]) < cv.int_(config[CONF_X_MIN]) or cv.int_(
        config[CONF_Y_MAX]
    ) < cv.int_(config[CONF_Y_MIN]):
        raise cv.Invalid("x_max is less than x_min or y_max is less than y_min")

    return config


CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(XPT2046Button).extend(
        {
            cv.GenerateID(CONF_XPT2046_ID): cv.use_id(XPT2046Component),
            cv.Required(CONF_X_MIN): cv.int_range(min=0, max=4095),
            cv.Required(CONF_X_MAX): cv.int_range(min=0, max=4095),
            cv.Required(CONF_Y_MIN): cv.int_range(min=0, max=4095),
            cv.Required(CONF_Y_MAX): cv.int_range(min=0, max=4095),
        }
    ),
    validate_xpt2046_button,
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    hub = await cg.get_variable(config[CONF_XPT2046_ID])
    cg.add(
        var.set_area(
            config[CONF_X_MIN],
            config[CONF_X_MAX],
            config[CONF_Y_MIN],
            config[CONF_Y_MAX],
        )
    )

    cg.add(hub.register_button(var))
