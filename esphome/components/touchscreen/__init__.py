import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.components import display
from esphome import automation
from esphome.const import CONF_ON_TOUCH, CONF_DIMENSIONS, CONF_ROTATION
from esphome.core import coroutine_with_priority

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["display"]

IS_PLATFORM_COMPONENT = True

touchscreen_ns = cg.esphome_ns.namespace("touchscreen")

Touchscreen = touchscreen_ns.class_("Touchscreen")
TouchRotation = touchscreen_ns.enum("TouchRotation")
TouchPoint = touchscreen_ns.struct("TouchPoint")
TouchListener = touchscreen_ns.class_("TouchListener")

CONF_DISPLAY = "display"
CONF_TOUCHSCREEN_ID = "touchscreen_id"


DISPLAY_ROTATIONS = {
    0: touchscreen_ns.ROTATE_0_DEGREES,
    90: touchscreen_ns.ROTATE_90_DEGREES,
    180: touchscreen_ns.ROTATE_180_DEGREES,
    270: touchscreen_ns.ROTATE_2700_DEGREES,
}


def validate_rotation(value):
    value = cv.string(value)
    if value.endswith("°"):
        value = value[:-1]
    return cv.enum(DISPLAY_ROTATIONS, int=True)(value)


TOUCHSCREEN_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DISPLAY): cv.use_id(display.DisplayBuffer),
        cv.Optional(CONF_ON_TOUCH): automation.validate_automation(single=True),
        cv.Optional(CONF_DIMENSIONS): cv.dimensions,
        cv.Optional(CONF_ROTATION): validate_rotation,
    }
)


async def register_touchscreen(var, config):
    if CONF_DISPLAY in config:
        disp = await cg.get_variable(config[CONF_DISPLAY])
        cg.add(var.set_display(disp))
    else:
        if CONF_DIMENSIONS in config:
            cg.add(
                var.set_display_dimension(
                    config[CONF_DIMENSIONS][0], config[CONF_DIMENSIONS][1]
                )
            )
        if CONF_ROTATION in config:
            cg.add(var.set_rotation(config[CONF_ROTATION]))

    if CONF_ON_TOUCH in config:
        await automation.build_automation(
            var.get_touch_trigger(),
            [(TouchPoint, "touch")],
            config[CONF_ON_TOUCH],
        )


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_global(touchscreen_ns.using)
    cg.add_define("USE_TOUCHSCREEN")
