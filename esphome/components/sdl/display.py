from collections.abc import Callable
import subprocess
from typing import Any

import esphome.codegen as cg
from esphome.components import display
from esphome.components.snapshot import Snapshot, register_snapshot
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
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import SDL_KEYMAP

AUTO_LOAD = ["snapshot"]

sdl_ns = cg.esphome_ns.namespace("sdl")
Sdl = sdl_ns.class_("Sdl", display.Display, cg.Component, Snapshot)
sdl_window_flags = cg.global_ns.enum("SDL_WindowFlags")


CONF_CENTERED_ON_DISPLAY = "centered_on_display"
CONF_HEADLESS = "headless"
CONF_SNAPSHOT_KEY = "snapshot_key"
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

SDL_WINDOWPOS_CENTERED_MASK = 0x2FFF0000


def get_sdl_options(value: str) -> str:
    if value != "":
        return value
    try:
        return subprocess.check_output(
            ["sdl2-config", "--cflags", "--libs"], close_fds=False
        ).decode()
    except Exception as e:
        raise cv.Invalid("Unable to run sdl2-config - have you installed sdl2?") from e


def get_window_options() -> dict[cv.Optional, Callable[[Any], Any]]:
    return {cv.Optional(option, default=False): cv.boolean for option in WINDOW_OPTIONS}


def _validate_position(config: dict) -> dict:
    if CONF_CENTERED_ON_DISPLAY in config:
        if CONF_X in config or CONF_Y in config:
            raise cv.Invalid(
                f"Cannot specify '{CONF_CENTERED_ON_DISPLAY}' with '{CONF_X}' and '{CONF_Y}' options"
            )
        return config
    if CONF_X in config and CONF_Y in config:
        return config
    if CONF_X in config or CONF_Y in config:
        raise cv.Invalid(f"Must specify both '{CONF_X}' and '{CONF_Y}' options")
    raise cv.Invalid("Must specify either 'x' and 'y' or 'centered_on_display'")


def _validate_headless(config: ConfigType) -> ConfigType:
    if not config[CONF_HEADLESS]:
        return config
    if CONF_WINDOW_OPTIONS in config:
        raise cv.Invalid(
            f"'{CONF_WINDOW_OPTIONS}' has no effect when '{CONF_HEADLESS}' is set - there is no window"
        )
    if CONF_SNAPSHOT_KEY in config:
        raise cv.Invalid(
            f"'{CONF_SNAPSHOT_KEY}' cannot be used when '{CONF_HEADLESS}' is set - "
            f"there is no keyboard. Use the 'snapshot.take' action instead"
        )
    return config


CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(Sdl),
                cv.Optional(CONF_SDL_OPTIONS, default=""): get_sdl_options,
                cv.Optional(CONF_HEADLESS, default=False): cv.boolean,
                cv.Optional(CONF_SNAPSHOT_KEY): cv.enum(SDL_KEYMAP),
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
                                cv.Optional(CONF_X): cv.int_,
                                cv.Optional(CONF_Y): cv.int_,
                                cv.Optional(CONF_CENTERED_ON_DISPLAY): cv.int_range(
                                    0, 128
                                ),
                            }
                        ).add_extra(_validate_position),
                        **get_window_options(),
                    }
                ),
            }
        )
    ),
    _validate_headless,
    cv.only_on(PLATFORM_HOST),
)


def headless_final_validate(platform: str) -> cv.Schema:
    """Build a FINAL_VALIDATE_SCHEMA rejecting a platform whose sdl display is headless.

    Mouse and keyboard platforms are driven by window events, so under a headless display they
    would never report anything.
    """

    def validate_display(display_config: ConfigType) -> ConfigType:
        if display_config.get(CONF_HEADLESS):
            raise cv.Invalid(
                f"The sdl {platform} platform needs a window, but its display has "
                f"'{CONF_HEADLESS}' set"
            )
        return display_config

    return cv.Schema(
        {cv.Required(CONF_SDL_ID): fv.id_declaration_match_schema(validate_display)},
        extra=cv.ALLOW_EXTRA,
    )


async def to_code(config: ConfigType) -> None:
    for option in config[CONF_SDL_OPTIONS].split():
        cg.add_build_flag(option)
    cg.add_build_flag("-DSDL_BYTEORDER=4321")
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    await register_snapshot(var, config)
    cg.add(var.set_headless(config[CONF_HEADLESS]))
    if (key := config.get(CONF_SNAPSHOT_KEY)) is not None:
        cg.add(var.set_snapshot_key(key))

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
            if (centered := position.get(CONF_CENTERED_ON_DISPLAY)) is not None:
                cg.add(
                    var.set_position(
                        SDL_WINDOWPOS_CENTERED_MASK | centered,
                        SDL_WINDOWPOS_CENTERED_MASK | centered,
                    )
                )
            else:
                cg.add(var.set_position(position[CONF_X], position[CONF_Y]))

    if lamb := config.get(CONF_LAMBDA):
        lambda_ = await cg.process_lambda(
            lamb, [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
