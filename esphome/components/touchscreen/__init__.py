import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.components import display
from esphome import automation
from esphome.const import CONF_ON_TOUCH
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


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_global(touchscreen_ns.using)
    cg.add_define("USE_TOUCHSCREEN")
