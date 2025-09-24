from esphome.components.text_sensor import (
    TextSensor,
    new_text_sensor,
    text_sensor_schema,
)
import esphome.config_validation as cv

from ..defines import CONF_WIDGET
from ..lvcode import (
    API_EVENT,
    EVENT_ARG,
    UPDATE_EVENT,
    LambdaContext,
    LvContext,
    lvgl_static,
)
from ..types import LV_EVENT, LvText
from ..widgets import get_widgets, wait_for_widgets

CONFIG_SCHEMA = text_sensor_schema(TextSensor).extend(
    {
        cv.Required(CONF_WIDGET): cv.use_id(LvText),
    }
)


async def to_code(config):
    sensor = await new_text_sensor(config)
    widget = await get_widgets(config, CONF_WIDGET)
    widget = widget[0]
    await wait_for_widgets()
    async with LambdaContext(EVENT_ARG) as pressed_ctx:
        pressed_ctx.add(sensor.publish_state(widget.get_value()))
    async with LvContext() as ctx:
        ctx.add(
            lvgl_static.add_event_cb(
                widget.obj,
                await pressed_ctx.get_lambda(),
                LV_EVENT.VALUE_CHANGED,
                API_EVENT,
                UPDATE_EVENT,
            )
        )
