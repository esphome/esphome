import esphome.codegen as cg
from esphome.components import touchscreen
import esphome.config_validation as cv
from esphome.const import CONF_ID

from ..display import CONF_SDL_ID, Sdl, sdl_ns

SdlTouchscreen = sdl_ns.class_("SdlTouchscreen", touchscreen.Touchscreen)


CONFIG_SCHEMA = touchscreen.TOUCHSCREEN_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SdlTouchscreen),
        cv.GenerateID(CONF_SDL_ID): cv.use_id(Sdl),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_SDL_ID])
    await touchscreen.register_touchscreen(var, config)
