import esphome.codegen as cg
from esphome.components import text
from esphome.components.text import new_text
import esphome.config_validation as cv

from ..defines import CONF_LVGL_ID, CONF_WIDGET
from ..lvcode import CUSTOM_EVENT, EVENT_ARG, LambdaContext, LvContext, lv, lv_add
from ..schemas import LVGL_SCHEMA
from ..types import LV_EVENT, LvText, lvgl_ns
from ..widgets import get_widgets

LVGLText = lvgl_ns.class_("LVGLText", text.Text)

CONFIG_SCHEMA = text.TEXT_SCHEMA.extend(LVGL_SCHEMA).extend(
    {
        cv.GenerateID(): cv.declare_id(LVGLText),
        cv.Required(CONF_WIDGET): cv.use_id(LvText),
    }
)


async def to_code(config):
    textvar = await new_text(config)
    paren = await cg.get_variable(config[CONF_LVGL_ID])
    widget = await get_widgets(config, CONF_WIDGET)
    widget = widget[0]
    async with LambdaContext([(cg.std_string, "text_value")]) as control:
        await widget.set_property("text", "text_value.c_str())")
        lv.event_send(widget.obj, CUSTOM_EVENT, None)
    async with LambdaContext(EVENT_ARG) as lamb:
        lv_add(textvar.publish_state(widget.get_value()))
    async with LvContext(paren):
        widget.var.set_control_lambda(await control.get_lambda())
        lv_add(
            paren.add_event_cb(
                widget.obj, await lamb.get_lambda(), LV_EVENT.VALUE_CHANGED
            )
        )
        lv_add(textvar.publish_state(widget.get_value()))
