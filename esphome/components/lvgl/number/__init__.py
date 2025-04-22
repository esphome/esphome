import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_RESTORE_VALUE
from esphome.cpp_generator import MockObj

from ..defines import CONF_ANIMATED, CONF_UPDATE_ON_RELEASE, CONF_WIDGET
from ..lv_validation import animated
from ..lvcode import (
    API_EVENT,
    EVENT_ARG,
    UPDATE_EVENT,
    LambdaContext,
    ReturnStatement,
    lv,
    lvgl_static,
)
from ..types import LV_EVENT, LvNumber, lvgl_ns
from ..widgets import get_widgets, wait_for_widgets

LVGLNumber = lvgl_ns.class_("LVGLNumber", number.Number, cg.Component)

CONFIG_SCHEMA = number.number_schema(LVGLNumber).extend(
    {
        cv.Required(CONF_WIDGET): cv.use_id(LvNumber),
        cv.Optional(CONF_ANIMATED, default=True): animated,
        cv.Optional(CONF_UPDATE_ON_RELEASE, default=False): cv.boolean,
        cv.Optional(CONF_RESTORE_VALUE, default=False): cv.boolean,
    }
)


async def to_code(config):
    widget = await get_widgets(config, CONF_WIDGET)
    widget = widget[0]
    await wait_for_widgets()
    async with LambdaContext([], return_type=cg.float_) as value:
        value.add(ReturnStatement(widget.get_value()))
    async with LambdaContext([(cg.float_, "v")]) as control:
        await widget.set_property(
            "value", MockObj("v") * MockObj(widget.get_scale()), config[CONF_ANIMATED]
        )
        lv.event_send(widget.obj, API_EVENT, cg.nullptr)
    event_code = (
        LV_EVENT.VALUE_CHANGED
        if not config[CONF_UPDATE_ON_RELEASE]
        else LV_EVENT.RELEASED
    )
    var = await number.new_number(
        config,
        await control.get_lambda(),
        await value.get_lambda(),
        event_code,
        config[CONF_RESTORE_VALUE],
        max_value=widget.get_max(),
        min_value=widget.get_min(),
        step=widget.get_step(),
    )
    async with LambdaContext(EVENT_ARG) as event:
        event.add(var.on_value())
    await cg.register_component(var, config)
    cg.add(
        lvgl_static.add_event_cb(
            widget.obj, await event.get_lambda(), UPDATE_EVENT, event_code
        )
    )
