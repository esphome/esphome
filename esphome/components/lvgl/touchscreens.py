import esphome.codegen as cg
from esphome.components.touchscreen import CONF_TOUCHSCREEN_ID, Touchscreen
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE

from .defines import (
    CONF_LONG_PRESS_REPEAT_TIME,
    CONF_LONG_PRESS_TIME,
    CONF_TOUCHSCREENS,
)
from .helpers import lvgl_components_required
from .lvcode import lv
from .schemas import PRESS_TIME
from .types import LVTouchListener

CONF_TOUCHSCREEN = "touchscreen"
TOUCHSCREENS_CONFIG = cv.maybe_simple_value(
    {
        cv.Required(CONF_TOUCHSCREEN_ID): cv.use_id(Touchscreen),
        cv.Optional(CONF_LONG_PRESS_TIME, default="400ms"): PRESS_TIME,
        cv.Optional(CONF_LONG_PRESS_REPEAT_TIME, default="100ms"): PRESS_TIME,
        cv.GenerateID(): cv.declare_id(LVTouchListener),
    },
    key=CONF_TOUCHSCREEN_ID,
)


def touchscreen_schema(config):
    value = cv.ensure_list(TOUCHSCREENS_CONFIG)(config)
    if value or CONF_TOUCHSCREEN not in CORE.loaded_integrations:
        return value
    return [TOUCHSCREENS_CONFIG(config)]


async def touchscreens_to_code(var, config):
    for tconf in config.get(CONF_TOUCHSCREENS) or ():
        lvgl_components_required.add(CONF_TOUCHSCREEN)
        touchscreen = await cg.get_variable(tconf[CONF_TOUCHSCREEN_ID])
        lpt = tconf[CONF_LONG_PRESS_TIME].total_milliseconds
        lprt = tconf[CONF_LONG_PRESS_REPEAT_TIME].total_milliseconds
        listener = cg.new_Pvariable(tconf[CONF_ID], lpt, lprt)
        await cg.register_parented(listener, var)
        lv.indev_drv_register(listener.get_drv())
        cg.add(touchscreen.register_listener(listener))
