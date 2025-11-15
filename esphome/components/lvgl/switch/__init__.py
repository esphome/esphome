import esphome.codegen as cg
from esphome.components.switch import Switch, register_switch, switch_schema
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import MockObj
from esphome.cpp_types import Component

from ..defines import CONF_WIDGET, literal
from ..lvcode import (
    API_EVENT,
    EVENT_ARG,
    UPDATE_EVENT,
    LambdaContext,
    LvConditional,
    LvContext,
    lv,
    lv_add,
    lvgl_static,
)
from ..types import LV_EVENT, LV_STATE, lv_pseudo_button_t, lvgl_ns
from ..widgets import get_widgets, wait_for_widgets

LVGLSwitch = lvgl_ns.class_("LVGLSwitch", Switch, Component)
CONFIG_SCHEMA = switch_schema(LVGLSwitch).extend(
    {
        cv.Required(CONF_WIDGET): cv.use_id(lv_pseudo_button_t),
    }
)


async def to_code(config):
    widget = await get_widgets(config, CONF_WIDGET)
    widget = widget[0]
    await wait_for_widgets()
    switch_id = MockObj(config[CONF_ID], "->")
    v = literal("v")
    async with LambdaContext([(cg.bool_, "v")]) as control:
        with LvConditional(v) as cond:
            widget.add_state(LV_STATE.CHECKED)
            cond.else_()
            widget.clear_state(LV_STATE.CHECKED)
        lv.event_send(widget.obj, API_EVENT, cg.nullptr)
        control.add(switch_id.publish_state(v))
    switch = cg.new_Pvariable(config[CONF_ID], await control.get_lambda())
    await cg.register_component(switch, config)
    await register_switch(switch, config)
    async with LambdaContext(EVENT_ARG) as checked_ctx:
        checked_ctx.add(switch.publish_state(widget.get_value()))
    async with LvContext() as ctx:
        ctx.add(
            lvgl_static.add_event_cb(
                widget.obj,
                await checked_ctx.get_lambda(),
                LV_EVENT.VALUE_CHANGED,
                UPDATE_EVENT,
            )
        )
        lv_add(switch.publish_state(widget.get_value()))
