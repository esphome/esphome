from esphome.components.binary_sensor import (
    BinarySensor,
    binary_sensor_schema,
    new_binary_sensor,
)
import esphome.config_validation as cv

from ..defines import CONF_WIDGET
from ..lvcode import EVENT_ARG, LambdaContext, LvContext, lvgl_static
from ..types import LV_EVENT, lv_pseudo_button_t
from ..widgets import Widget, get_widgets, wait_for_widgets

CONFIG_SCHEMA = binary_sensor_schema(BinarySensor).extend(
    {
        cv.Required(CONF_WIDGET): cv.use_id(lv_pseudo_button_t),
    }
)


async def to_code(config):
    sensor = await new_binary_sensor(config)
    widget = await get_widgets(config, CONF_WIDGET)
    widget = widget[0]
    assert isinstance(widget, Widget)
    await wait_for_widgets()
    async with LambdaContext(EVENT_ARG) as pressed_ctx:
        pressed_ctx.add(sensor.publish_state(widget.is_pressed()))
    async with LvContext() as ctx:
        ctx.add(sensor.publish_initial_state(widget.is_pressed()))
        ctx.add(
            lvgl_static.add_event_cb(
                widget.obj,
                await pressed_ctx.get_lambda(),
                LV_EVENT.PRESSING,
                LV_EVENT.RELEASED,
            )
        )
