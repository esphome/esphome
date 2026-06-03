import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_ON_RELEASE, CONF_RESTORE_VALUE
from esphome.cpp_generator import MockObj

from ..defines import (
    CONF_ANIMATED,
    CONF_TRIGGER,
    CONF_UPDATE_ON_RELEASE,
    CONF_WIDGET,
    LOGGER,
)
from ..lv_validation import animated
from ..lvcode import (
    EVENT_ARG,
    UPDATE_EVENT,
    LambdaContext,
    ReturnStatement,
    lv_obj,
    lvgl_static,
)
from ..schemas import TRIGGER_EVENT_MAP, VALUE_TRIGGER_SCHEMA
from ..types import LvNumber, lvgl_ns
from ..widgets import get_widgets, wait_for_widgets

LVGLNumber = lvgl_ns.class_("LVGLNumber", number.Number, cg.Component)

CONFIG_SCHEMA = number.number_schema(LVGLNumber).extend(
    {
        cv.Required(CONF_WIDGET): cv.use_id(LvNumber),
        **VALUE_TRIGGER_SCHEMA,
        cv.Optional(CONF_ANIMATED, default=True): animated,
        cv.Optional(CONF_UPDATE_ON_RELEASE): cv.boolean,
        cv.Optional(CONF_RESTORE_VALUE, default=False): cv.boolean,
    }
)


async def to_code(config):
    trigger = config[CONF_TRIGGER]
    if CONF_UPDATE_ON_RELEASE in config:
        LOGGER.warning(
            "Option 'update_on_release' is deprecated and will be removed in 2026.11.0 - use 'trigger: on_release' instead"
        )
        if config[CONF_UPDATE_ON_RELEASE]:
            trigger = CONF_ON_RELEASE
    widget = await get_widgets(config, CONF_WIDGET)
    widget = widget[0]
    await wait_for_widgets()
    async with LambdaContext([], return_type=cg.float_) as value:
        value.add(ReturnStatement(widget.get_value()))
    async with LambdaContext([(cg.float_, "v")]) as control:
        await widget.set_property(
            "value", MockObj("v") * MockObj(widget.get_scale()), config[CONF_ANIMATED]
        )
        lv_obj.send_event(widget.obj, UPDATE_EVENT, cg.nullptr)
    var = await number.new_number(
        config,
        await control.get_lambda(),
        await value.get_lambda(),
        config[CONF_RESTORE_VALUE],
        max_value=await widget.type.get_max(widget.config),
        min_value=await widget.type.get_min(widget.config),
        step=widget.type.get_step(widget.config),
    )
    async with LambdaContext(EVENT_ARG) as event:
        event.add(var.on_value())
    await cg.register_component(var, config)
    cg.add(
        lvgl_static.add_event_cb(
            widget.obj,
            await event.get_lambda(),
            *TRIGGER_EVENT_MAP[trigger],
        )
    )
