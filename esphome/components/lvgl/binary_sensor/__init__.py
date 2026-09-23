from esphome.components.binary_sensor import (
    BinarySensor,
    binary_sensor_schema,
    new_binary_sensor,
)
import esphome.config_validation as cv
from esphome.const import CONF_STATE

from ..defines import CONF_WIDGET, LV_OBJ_FLAG, LvConstant
from ..lvcode import EVENT_ARG, UPDATE_EVENT, LambdaContext, LvContext, lvgl_static
from ..types import LV_EVENT, LV_STATE, lv_pseudo_button_t
from ..widgets import Widget, get_widgets, wait_for_widgets

STATE_PRESSED = "PRESSED"
STATE_CHECKED = "CHECKED"

BS_STATE = LvConstant(
    "LV_STATE_",
    STATE_PRESSED,
    STATE_CHECKED,
)
CONFIG_SCHEMA = binary_sensor_schema(BinarySensor).extend(
    {
        cv.Required(CONF_WIDGET): cv.use_id(lv_pseudo_button_t),
        cv.Optional(CONF_STATE, default=STATE_PRESSED): BS_STATE.one_of,
    }
)


async def to_code(config):
    sensor = await new_binary_sensor(config)
    widget = await get_widgets(config, CONF_WIDGET)
    widget = widget[0]
    assert isinstance(widget, Widget)
    state = await BS_STATE.process(config[CONF_STATE])
    await wait_for_widgets()
    is_pressed = str(state) == str(LV_STATE.PRESSED)
    test_expr = widget.is_pressed() if is_pressed else widget.is_checked()
    async with LambdaContext(EVENT_ARG) as test_ctx:
        test_ctx.add(sensor.publish_state(test_expr))
    async with LvContext() as ctx:
        ctx.add(sensor.publish_initial_state(test_expr))
        if is_pressed:
            events = [LV_EVENT.PRESSED, LV_EVENT.RELEASED]
            widget.add_flag(LV_OBJ_FLAG.CLICKABLE)
        else:
            events = [LV_EVENT.VALUE_CHANGED, UPDATE_EVENT]
        ctx.add(
            lvgl_static.add_event_cb(
                widget.obj,
                await test_ctx.get_lambda(),
                *events,
            )
        )
