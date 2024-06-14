import subprocess

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display
from esphome.const import (
    CONF_ID,
    CONF_DIMENSIONS,
    CONF_WIDTH,
    CONF_HEIGHT,
    CONF_LAMBDA,
    PLATFORM_HOST,
)

sdl_ns = cg.esphome_ns.namespace("sdl")
Sdl = sdl_ns.class_("Sdl", display.Display, cg.Component)


CONF_SDL_OPTIONS = "sdl_options"
CONF_SDL_ID = "sdl_id"


def get_sdl_options(value):
    if value != "":
        return value
    try:
        return subprocess.check_output(["sdl2-config", "--cflags", "--libs"]).decode()
    except Exception as e:
        raise cv.Invalid("Unable to run sdl2-config - have you installed sdl2?") from e


CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(Sdl),
                cv.Optional(CONF_SDL_OPTIONS, default=""): get_sdl_options,
                cv.Required(CONF_DIMENSIONS): cv.Any(
                    cv.dimensions,
                    cv.Schema(
                        {
                            cv.Required(CONF_WIDTH): cv.int_,
                            cv.Required(CONF_HEIGHT): cv.int_,
                        }
                    ),
                ),
            }
        )
    ),
    cv.only_on(PLATFORM_HOST),
)


async def to_code(config):
    for option in config[CONF_SDL_OPTIONS].split():
        cg.add_build_flag(option)
    cg.add_build_flag("-DSDL_BYTEORDER=4321")
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)

    dimensions = config[CONF_DIMENSIONS]
    if isinstance(dimensions, dict):
        cg.add(var.set_dimensions(dimensions[CONF_WIDTH], dimensions[CONF_HEIGHT]))
    else:
        (width, height) = dimensions
        cg.add(var.set_dimensions(width, height))

    if lamb := config.get(CONF_LAMBDA):
        lambda_ = await cg.process_lambda(
            lamb, [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
