import esphome.codegen as cg
from esphome.components.touchscreen import CONF_TOUCHSCREEN_ID, Touchscreen
import esphome.config_validation as cv
from esphome.const import CONF_ID

from ...core import ID
from .defines import (
    CONF_LONG_PRESS_REPEAT_TIME,
    CONF_LONG_PRESS_TIME,
    CONF_TOUCHSCREENS,
)
from .helpers import add_lv_use
from .lv_validation import lv_milliseconds
from .lvcode import lv, lv_add
from .types import LVTouchListener

TOUCHSCREENS_CONFIG = cv.Any(
    cv.use_id(Touchscreen),
    cv.ensure_list(
        cv.maybe_simple_value(
            {
                cv.Required(CONF_TOUCHSCREEN_ID): cv.use_id(Touchscreen),
                cv.Optional(CONF_LONG_PRESS_TIME, default="400ms"): lv_milliseconds,
                cv.Optional(
                    CONF_LONG_PRESS_REPEAT_TIME, default="100ms"
                ): lv_milliseconds,
                cv.GenerateID(): cv.declare_id(LVTouchListener),
            },
            key=CONF_TOUCHSCREEN_ID,
        ),
    ),
)


async def add_touchscreen(var, id, listener):
    add_lv_use("TOUCHSCREEN")
    await cg.register_parented(listener, var)
    lv.indev_drv_register(listener.get_drv())
    lv_add(id.register_listener(listener))


async def touchscreens_to_code(var, config):
    if touchscreens := config.get(CONF_TOUCHSCREENS):
        if isinstance(touchscreens, list):
            for tconf in touchscreens:
                touchscreen = await cg.get_variable(tconf[CONF_TOUCHSCREEN_ID])
                lpt = tconf[CONF_LONG_PRESS_TIME].total_milliseconds & 0xFFFF
                lprt = tconf[CONF_LONG_PRESS_REPEAT_TIME].total_milliseconds & 0xFFFF
                listener = cg.new_Pvariable(tconf[CONF_ID], lpt, lprt)
                await add_touchscreen(var, touchscreen, listener)
        else:
            touchscreen = await cg.get_variable(touchscreens)
            listener = cg.new_Pvariable(
                ID(f"{touchscreens}_listener", type=LVTouchListener), 400, 100
            )
            await add_touchscreen(var, touchscreen, listener)
