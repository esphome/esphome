import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.cpp_generator import MockObj

from ..defines import CONF_ANIMATED, CONF_LVGL_ID, CONF_WIDGET
from ..lv_validation import animated
from ..lvcode import CUSTOM_EVENT, EVENT_ARG, LambdaContext, LvContext, lv, lv_add
from ..schemas import LVGL_SCHEMA
from ..types import LV_EVENT, LvNumber, lvgl_ns
from ..widgets import get_widgets

LVGLNumber = lvgl_ns.class_("LVGLNumber", number.Number)

CONFIG_SCHEMA = (
    number.number_schema(LVGLNumber)
    .extend(LVGL_SCHEMA)
    .extend(
        {
            cv.Required(CONF_WIDGET): cv.use_id(LvNumber),
            cv.Optional(CONF_ANIMATED, default=True): animated,
        }
    )
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_LVGL_ID])
    widget = await get_widgets(config, CONF_WIDGET)
    widget = widget[0]
    var = await number.new_number(
        config,
        max_value=widget.get_max(),
        min_value=widget.get_min(),
        step=widget.get_step(),
    )

    async with LambdaContext([(cg.float_, "v")]) as control:
        await widget.set_property(
            "value", MockObj("v") * MockObj(widget.get_scale()), config[CONF_ANIMATED]
        )
        lv.event_send(widget.obj, CUSTOM_EVENT, cg.nullptr)
    async with LambdaContext(EVENT_ARG) as event:
        event.add(var.publish_state(widget.get_value()))
    async with LvContext(paren):
        lv_add(var.set_control_lambda(await control.get_lambda()))
        lv_add(
            paren.add_event_cb(
                widget.obj, await event.get_lambda(), LV_EVENT.VALUE_CHANGED
            )
        )
        lv_add(var.publish_state(widget.get_value()))
