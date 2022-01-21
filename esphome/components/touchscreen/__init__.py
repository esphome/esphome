import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.components import display
from esphome import automation
from esphome.const import CONF_ON_TOUCH

CODEOWNERS = ["@jesserockz"]
IS_PLATFORM_COMPONENT = True

touchscreen_ns = cg.esphome_ns.namespace("touchscreen")

Touchscreen = touchscreen_ns.class_("Touchscreen")
TouchRotation = touchscreen_ns.enum("TouchRotation")
TouchPoint = touchscreen_ns.struct("TouchPoint")
TouchListener = touchscreen_ns.class_("TouchListener")

CONF_DISPLAY = "display"
CONF_TOUCHSCREEN_ID = "touchscreen_id"

ROTATIONS = {
    0: TouchRotation.ROTATE_0_DEGREES,
    90: TouchRotation.ROTATE_90_DEGREES,
    180: TouchRotation.ROTATE_180_DEGREES,
    270: TouchRotation.ROTATE_270_DEGREES,
}


def validate_rotation(value):
    value = cv.string(value)
    if value.endswith("°"):
        value = value[:-1]
    return cv.enum(ROTATIONS, int=True)(value)


TOUCHSCREEN_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DISPLAY): cv.use_id(display.DisplayBuffer),
        cv.Optional(CONF_ON_TOUCH): automation.validate_automation(single=True),
    }
)


async def register_touchscreen(var, config):
    disp = await cg.get_variable(config[CONF_DISPLAY])
    cg.add(var.set_display(disp))

    if CONF_ON_TOUCH in config:
        await automation.build_automation(
            var.get_touch_trigger(),
            [(TouchPoint, "touch")],
            config[CONF_ON_TOUCH],
        )
