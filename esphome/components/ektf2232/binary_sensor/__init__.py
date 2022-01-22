import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import binary_sensor
from esphome.const import CONF_ID

from .. import ektf2232_ns, CONF_EKTF2232_ID, EKTF2232Touchscreen, TouchListener

DEPENDENCIES = ["ektf2232"]

EKTF2232Button = ektf2232_ns.class_(
    "EKTF2232Button", binary_sensor.BinarySensor, TouchListener
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
    binary_sensor.BINARY_SENSOR_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(EKTF2232Button),
            cv.GenerateID(CONF_EKTF2232_ID): cv.use_id(EKTF2232Touchscreen),
            cv.Required(CONF_X_MIN): cv.int_range(min=0, max=2000),
            cv.Required(CONF_X_MAX): cv.int_range(min=0, max=2000),
            cv.Required(CONF_Y_MIN): cv.int_range(min=0, max=2000),
            cv.Required(CONF_Y_MAX): cv.int_range(min=0, max=2000),
        }
    ),
    validate_coords,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await binary_sensor.register_binary_sensor(var, config)
    hub = await cg.get_variable(config[CONF_EKTF2232_ID])
    cg.add(
        var.set_area(
            config[CONF_X_MIN],
            config[CONF_X_MAX],
            config[CONF_Y_MIN],
            config[CONF_Y_MAX],
        )
    )
    cg.add(hub.register_listener(var))
