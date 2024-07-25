import esphome.codegen as cg
from esphome.components.touchscreen import CONF_TOUCHSCREEN_ID, Touchscreen
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .defines import (
    CONF_LONG_PRESS_REPEAT_TIME,
    CONF_LONG_PRESS_TIME,
    CONF_TOUCHSCREENS,
)
from .helpers import add_lv_use
from .lv_validation import lv_milliseconds
from .lvcode import lv, lv_add
from .types import LVTouchListener

TOUCHSCREENS_CONFIG = cv.maybe_simple_value(
    {
        cv.Required(CONF_TOUCHSCREEN_ID): cv.use_id(Touchscreen),
        cv.Optional(CONF_LONG_PRESS_TIME, default="400ms"): lv_milliseconds,
        cv.Optional(CONF_LONG_PRESS_REPEAT_TIME, default="100ms"): lv_milliseconds,
        cv.GenerateID(): cv.declare_id(LVTouchListener),
    },
    key=CONF_TOUCHSCREEN_ID,
)


def touchscreen_config(config):
    value = cv.ensure_list(TOUCHSCREENS_CONFIG)(config)
    return value or [TOUCHSCREENS_CONFIG(config)]


async def touchscreens_to_code(var, config):
    for tconf in config.get(CONF_TOUCHSCREENS) or ():
        add_lv_use("TOUCHSCREEN")
        touchscreen = await cg.get_variable(tconf[CONF_TOUCHSCREEN_ID])
        lpt = tconf[CONF_LONG_PRESS_TIME].total_milliseconds & 0xFFFF
        lprt = tconf[CONF_LONG_PRESS_REPEAT_TIME].total_milliseconds & 0xFFFF
        listener = cg.new_Pvariable(tconf[CONF_ID], lpt, lprt)
        await cg.register_parented(listener, var)
        lv.indev_drv_register(listener.get_drv())
        lv_add(touchscreen.register_listener(listener))
