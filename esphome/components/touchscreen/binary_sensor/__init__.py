import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import binary_sensor, display
from esphome.const import CONF_PAGE_ID

from .. import touchscreen_ns, CONF_TOUCHSCREEN_ID, Touchscreen, TouchListener

DEPENDENCIES = ["touchscreen"]

TouchscreenBinarySensor = touchscreen_ns.class_(
    "TouchscreenBinarySensor",
    binary_sensor.BinarySensor,
    cg.Component,
    TouchListener,
    cg.Parented.template(Touchscreen),
)

CONF_X_MIN = "x_min"
CONF_X_MAX = "x_max"
CONF_Y_MIN = "y_min"
CONF_Y_MAX = "y_max"


def validate_coords(config):
    if (
        config[CONF_X_MAX] < config[CONF_X_MIN]
        or config[CONF_Y_MAX] < config[CONF_Y_MIN]
    ):
        raise cv.Invalid(
            f"{CONF_X_MAX} is less than {CONF_X_MIN} or {CONF_Y_MAX} is less than {CONF_Y_MIN}"
        )
    return config


CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(TouchscreenBinarySensor)
    .extend(
        {
            cv.GenerateID(CONF_TOUCHSCREEN_ID): cv.use_id(Touchscreen),
            cv.Required(CONF_X_MIN): cv.int_range(min=0, max=2000),
            cv.Required(CONF_X_MAX): cv.int_range(min=0, max=2000),
            cv.Required(CONF_Y_MIN): cv.int_range(min=0, max=2000),
            cv.Required(CONF_Y_MAX): cv.int_range(min=0, max=2000),
            cv.Optional(CONF_PAGE_ID): cv.use_id(display.DisplayPage),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    validate_coords,
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_TOUCHSCREEN_ID])

    cg.add(
        var.set_area(
            config[CONF_X_MIN],
            config[CONF_X_MAX],
            config[CONF_Y_MIN],
            config[CONF_Y_MAX],
        )
    )

    if CONF_PAGE_ID in config:
        page = await cg.get_variable(config[CONF_PAGE_ID])
        cg.add(var.set_page(page))
