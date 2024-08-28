import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_OPTIONS

from ..defines import CONF_ANIMATED, CONF_LVGL_ID, CONF_WIDGET
from ..lvcode import (
    API_EVENT,
    EVENT_ARG,
    UPDATE_EVENT,
    LambdaContext,
    LvContext,
    lv,
    lv_add,
)
from ..schemas import LVGL_SCHEMA
from ..types import LV_EVENT, LvSelect, lvgl_ns
from ..widgets import get_widgets, wait_for_widgets

LVGLSelect = lvgl_ns.class_("LVGLSelect", select.Select)

CONFIG_SCHEMA = (
    select.select_schema(LVGLSelect)
    .extend(LVGL_SCHEMA)
    .extend(
        {
            cv.Required(CONF_WIDGET): cv.use_id(LvSelect),
            cv.Optional(CONF_ANIMATED, default=False): cv.boolean,
        }
    )
)


async def to_code(config):
    widget = await get_widgets(config, CONF_WIDGET)
    widget = widget[0]
    options = widget.config.get(CONF_OPTIONS, [])
    selector = await select.new_select(config, options=options)
    paren = await cg.get_variable(config[CONF_LVGL_ID])
    await wait_for_widgets()
    async with LambdaContext(EVENT_ARG) as pub_ctx:
        pub_ctx.add(selector.publish_index(widget.get_value()))
    async with LambdaContext([(cg.uint16, "v")]) as control:
        await widget.set_property("selected", "v", animated=config[CONF_ANIMATED])
        lv.event_send(widget.obj, API_EVENT, cg.nullptr)
        control.add(selector.publish_index(widget.get_value()))
    async with LvContext(paren) as ctx:
        lv_add(selector.set_control_lambda(await control.get_lambda()))
        ctx.add(
            paren.add_event_cb(
                widget.obj,
                await pub_ctx.get_lambda(),
                LV_EVENT.VALUE_CHANGED,
                UPDATE_EVENT,
            )
        )
        lv_add(selector.publish_index(widget.get_value()))
