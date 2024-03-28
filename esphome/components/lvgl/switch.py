import esphome.codegen as cg
from esphome.const import CONF_OUTPUT_ID
from esphome.components.output import BinaryOutput
import esphome.config_validation as cv
from esphome.components.switch import (
    switch_schema,
    Switch,
    new_switch,
)
from . import (
    add_init_lambda,
    LVGL_SCHEMA,
    CONF_LVGL_ID,
    lvgl_ns,
    lv_pseudo_button_t,
    CONF_WIDGET,
    get_widget,
)
from .lv_validation import requires_component

AUTO_LOAD = ["output"]

LVGLSwitch = lvgl_ns.class_("LVGLSwitch", Switch)
BASE_SCHEMA = switch_schema(LVGLSwitch).extend(LVGL_SCHEMA)
CONFIG_SCHEMA = cv.All(
    BASE_SCHEMA.extend(
        {
            cv.Required(CONF_WIDGET): cv.use_id(lv_pseudo_button_t),
            cv.Optional(CONF_OUTPUT_ID): cv.use_id(BinaryOutput),
        }
    ),
    requires_component("switch"),
)


async def to_code(config):
    switch = await new_switch(config)
    paren = await cg.get_variable(config[CONF_LVGL_ID])
    widget = await get_widget(config[CONF_WIDGET])
    output = config.get(CONF_OUTPUT_ID)
    publish = [
        f'bool v = {widget.has_state("LV_STATE_CHECKED")}',
        f"{switch}->publish_state(v)",
    ]
    if output:
        publish.append(f"{output}->set_state(v)")
    publish = ";".join(publish)
    init = widget.set_event_cb(publish, "LV_EVENT_VALUE_CHANGED")
    set_state = widget.set_state("LV_STATE_CHECKED", "v")
    set_state.append(f"{switch}->publish_state(v)")
    if output:
        set_state.append(f"{output}->set_state(v)")
    set_state = ";".join(set_state)
    init.append(f"{switch}->set_state_lambda([] (bool v) {{\n" + set_state + ";\n})")
    await add_init_lambda(paren, init)
