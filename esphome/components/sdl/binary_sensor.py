import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.components.binary_sensor import BinarySensor
import esphome.config_validation as cv
from esphome.const import CONF_KEY
from esphome.core import Lambda
from esphome.cpp_generator import ExpressionStatement, RawExpression
from esphome.types import ConfigType

from . import SDL_KEYMAP
from .display import CONF_SDL_ID, Sdl, headless_final_validate

CODEOWNERS = ["@bdm310"]

STATE_ARG = "state"

FINAL_VALIDATE_SCHEMA = headless_final_validate("binary_sensor")


CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(BinarySensor)
    .extend(
        {
            cv.Required(CONF_KEY): cv.enum(SDL_KEYMAP),
            cv.GenerateID(CONF_SDL_ID): cv.use_id(Sdl),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    parent = await cg.get_variable(config[CONF_SDL_ID])
    listener = Lambda(
        str(ExpressionStatement(var.publish_state(RawExpression(STATE_ARG))))
    )
    listener = await cg.process_lambda(listener, [(cg.bool_, STATE_ARG)])
    cg.add(parent.add_key_listener(config[CONF_KEY], listener))
