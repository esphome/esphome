import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.rotary_encoder.sensor import RotaryEncoderSensor
import esphome.config_validation as cv
from esphome.const import CONF_MAX_VALUE, CONF_MIN_VALUE, ICON_ROTATE_RIGHT, UNIT_STEPS

from .display import CONF_SDL_ID, Sdl

AUTO_LOAD = ["rotary_encoder"]
CONF_WRAP = "wrap"

sdl_ns = cg.esphome_ns.namespace("sdl")
SdlEncoder = sdl_ns.class_("SdlEncoder", RotaryEncoderSensor)


def validate_min_max_value(config):
    if CONF_MIN_VALUE in config and CONF_MAX_VALUE in config:
        min_val = config[CONF_MIN_VALUE]
        max_val = config[CONF_MAX_VALUE]
        if min_val >= max_val:
            raise cv.Invalid(
                f"Max value {max_val} must be greater than min value {min_val}"
            )
    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        SdlEncoder,
        unit_of_measurement=UNIT_STEPS,
        icon=ICON_ROTATE_RIGHT,
        accuracy_decimals=0,
    )
    .extend(
        {
            cv.GenerateID(CONF_SDL_ID): cv.use_id(Sdl),
            cv.Optional(CONF_MIN_VALUE): cv.int_range(min=0, max=0x7FFFFFFF),
            cv.Optional(CONF_MAX_VALUE): cv.int_range(min=0, max=0x7FFFFFFF),
            cv.Optional(CONF_WRAP, default=True): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    validate_min_max_value,
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_SDL_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_wrap(config[CONF_WRAP]))
    if CONF_MIN_VALUE in config:
        cg.add(var.set_min_value(config[CONF_MIN_VALUE]))
    if CONF_MAX_VALUE in config:
        cg.add(var.set_max_value(config[CONF_MAX_VALUE]))
