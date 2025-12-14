import subprocess

import esphome.codegen as cg
from esphome.components import display
import esphome.config_validation as cv
from esphome.const import (
    CONF_DIMENSIONS,
    CONF_HEIGHT,
    CONF_ID,
    CONF_LAMBDA,
    CONF_POSITION,
    CONF_WIDTH,
    CONF_X,
    CONF_Y,
    PLATFORM_HOST,
)

sdl_ns = cg.esphome_ns.namespace("sdl")
Sdl = sdl_ns.class_("Sdl", display.Display, cg.Component)
sdl_window_flags = cg.global_ns.enum("SDL_WindowFlags")


CONF_SDL_OPTIONS = "sdl_options"
CONF_SDL_ID = "sdl_id"
CONF_WINDOW_OPTIONS = "window_options"
WINDOW_OPTIONS = (
    "borderless",
    "always_on_top",
    "fullscreen",
    "skip_taskbar",
    "resizable",
)


def get_sdl_options(value):
    if value != "":
        return value
    try:
        return subprocess.check_output(
            ["sdl2-config", "--cflags", "--libs"], close_fds=False
        ).decode()
    except Exception as e:
        raise cv.Invalid("Unable to run sdl2-config - have you installed sdl2?") from e


def get_window_options():
    return {cv.Optional(option, default=False): cv.boolean for option in WINDOW_OPTIONS}


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
                cv.Optional(CONF_WINDOW_OPTIONS): cv.Schema(
                    {
                        cv.Optional(CONF_POSITION): cv.Schema(
                            {
                                cv.Required(CONF_X): cv.int_,
                                cv.Required(CONF_Y): cv.int_,
                            }
                        ),
                        **get_window_options(),
                    }
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

    if window_options := config.get(CONF_WINDOW_OPTIONS):
        create_flags = 0
        for option in WINDOW_OPTIONS:
            value = window_options.get(option, False)
            if value:
                create_flags = create_flags | getattr(
                    sdl_window_flags, "SDL_WINDOW_" + option.upper()
                )
        cg.add(var.set_window_options(create_flags))

        if position := window_options.get(CONF_POSITION):
            cg.add(var.set_position(position[CONF_X], position[CONF_Y]))

    if lamb := config.get(CONF_LAMBDA):
        lambda_ = await cg.process_lambda(
            lamb, [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
