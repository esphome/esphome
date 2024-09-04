import esphome.codegen as cg
from esphome.components.switch import Switch, new_switch, switch_schema
import esphome.config_validation as cv
from esphome.cpp_generator import MockObj

from ..defines import CONF_LVGL_ID, CONF_WIDGET, literal
from ..lvcode import (
    API_EVENT,
    EVENT_ARG,
    UPDATE_EVENT,
    LambdaContext,
    LvConditional,
    LvContext,
    lv,
    lv_add,
)
from ..schemas import LVGL_SCHEMA
from ..types import LV_EVENT, LV_STATE, lv_pseudo_button_t, lvgl_ns
from ..widgets import get_widgets, wait_for_widgets

LVGLSwitch = lvgl_ns.class_("LVGLSwitch", Switch)
CONFIG_SCHEMA = (
    switch_schema(LVGLSwitch)
    .extend(LVGL_SCHEMA)
    .extend(
        {
            cv.Required(CONF_WIDGET): cv.use_id(lv_pseudo_button_t),
        }
    )
)


async def to_code(config):
    switch = await new_switch(config)
    paren = await cg.get_variable(config[CONF_LVGL_ID])
    widget = await get_widgets(config, CONF_WIDGET)
    widget = widget[0]
    await wait_for_widgets()
    async with LambdaContext(EVENT_ARG) as checked_ctx:
        checked_ctx.add(switch.publish_state(widget.get_value()))
    async with LambdaContext([(cg.bool_, "v")]) as control:
        with LvConditional(MockObj("v")) as cond:
            widget.add_state(LV_STATE.CHECKED)
            cond.else_()
            widget.clear_state(LV_STATE.CHECKED)
        lv.event_send(widget.obj, API_EVENT, cg.nullptr)
        control.add(switch.publish_state(literal("v")))
    async with LvContext(paren) as ctx:
        lv_add(switch.set_control_lambda(await control.get_lambda()))
        ctx.add(
            paren.add_event_cb(
                widget.obj,
                await checked_ctx.get_lambda(),
                LV_EVENT.VALUE_CHANGED,
                UPDATE_EVENT,
            )
        )
        lv_add(switch.publish_state(widget.get_value()))
