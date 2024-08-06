import esphome.codegen as cg
from esphome.components.text_sensor import (
    TextSensor,
    new_text_sensor,
    text_sensor_schema,
)
import esphome.config_validation as cv

from ..defines import CONF_LVGL_ID, CONF_WIDGET
from ..lvcode import EVENT_ARG, LambdaContext, LvContext
from ..schemas import LVGL_SCHEMA
from ..types import LV_EVENT, LvText
from ..widgets import get_widgets

CONFIG_SCHEMA = (
    text_sensor_schema(TextSensor)
    .extend(LVGL_SCHEMA)
    .extend(
        {
            cv.Required(CONF_WIDGET): cv.use_id(LvText),
        }
    )
)


async def to_code(config):
    sensor = await new_text_sensor(config)
    paren = await cg.get_variable(config[CONF_LVGL_ID])
    widget = await get_widgets(config, CONF_WIDGET)
    widget = widget[0]
    async with LambdaContext(EVENT_ARG) as pressed_ctx:
        pressed_ctx.add(sensor.publish_state(widget.get_value()))
    async with LvContext(paren) as ctx:
        ctx.add(
            paren.add_event_cb(
                widget.obj,
                await pressed_ctx.get_lambda(),
                LV_EVENT.VALUE_CHANGED,
            )
        )
