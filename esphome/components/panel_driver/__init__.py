import esphome.codegen as cg
import esphome.config_validation as cv

CODEOWNERS = ["@clydebarrow"]
IS_PLATFORM_COMPONENT = True

from esphome.const import (
    CONF_ID,
    CONF_DIMENSIONS,
    CONF_WIDTH,
    CONF_HEIGHT,
)

CONF_OFFSET_HEIGHT = "offset_height"
CONF_OFFSET_WIDTH = "offset_width"
CONF_TRANSFORM = "transform"
CONF_MIRROR_X = "mirror_x"
CONF_MIRROR_Y = "mirror_y"
CONF_SWAP_XY = "swap_xy"
CONF_COLOR_MODE = "color_mode"

panel_driver_ns = cg.esphome_ns.namespace("panel_driver")
PanelDriver = panel_driver_ns.class_("PanelDriver", cg.Component)
PANEL_DRIVER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DIMENSIONS): cv.Schema(
            {
                cv.Required(CONF_WIDTH): cv.int_,
                cv.Required(CONF_HEIGHT): cv.int_,
                cv.Optional(CONF_OFFSET_HEIGHT, default=0): cv.int_,
                cv.Optional(CONF_OFFSET_WIDTH, default=0): cv.int_,
            }
        ),
        cv.Optional(CONF_TRANSFORM): cv.Schema(
            {
                cv.Optional(CONF_SWAP_XY, default=False): cv.boolean,
                cv.Optional(CONF_MIRROR_X, default=False): cv.boolean,
                cv.Optional(CONF_MIRROR_Y, default=False): cv.boolean,
            }
        ),
    }
)


async def register_panel_driver(config, *args):
    print(config[CONF_ID])
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await cg.register_component(var, config)
    dimensions = config[CONF_DIMENSIONS]
    cg.add(var.set_height(dimensions[CONF_HEIGHT]))
    cg.add(var.set_width(dimensions[CONF_WIDTH]))
    cg.add(var.set_offset_height(dimensions[CONF_OFFSET_HEIGHT]))
    cg.add(var.set_offset_width(dimensions[CONF_OFFSET_WIDTH]))
    if transform := config.get(CONF_TRANSFORM):
        cg.add(var.set_swap_xy(transform[CONF_SWAP_XY]))
        cg.add(var.set_mirror_x(transform[CONF_MIRROR_X]))
        cg.add(var.set_mirror_y(transform[CONF_MIRROR_Y]))
    return var
