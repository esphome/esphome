import esphome.codegen as cg
from esphome.components.switch import Switch, new_switch, switch_schema
import esphome.config_validation as cv

from .. import LVGL_SCHEMA, add_init_lambda
from ..defines import CONF_LVGL_ID, CONF_WIDGET
from ..lv_validation import requires_component
from ..types import lv_pseudo_button_t, lvgl_ns
from ..widget import get_widget

LVGLSwitch = lvgl_ns.class_("LVGLSwitch", Switch)
BASE_SCHEMA = switch_schema(LVGLSwitch).extend(LVGL_SCHEMA)
CONFIG_SCHEMA = cv.All(
    BASE_SCHEMA.extend(
        {
            cv.Required(CONF_WIDGET): cv.use_id(lv_pseudo_button_t),
        }
    ),
    requires_component("switch"),
)


async def to_code(config):
    switch = await new_switch(config)
    paren = await cg.get_variable(config[CONF_LVGL_ID])
    widget = await get_widget(config[CONF_WIDGET])
    publish = [
        f'bool v = {widget.has_state("LV_STATE_CHECKED")}',
        f"{switch}->publish_state(v)",
    ]
    publish = ";".join(publish)
    init = widget.set_event_cb(publish, "LV_EVENT_VALUE_CHANGED")
    set_state = widget.set_state("LV_STATE_CHECKED", "v")
    set_state.append(f"{switch}->publish_state(v)")
    set_state = ";".join(set_state)
    init.append(f"{switch}->set_state_lambda([] (bool v) {{\n" + set_state + ";\n})")
    await add_init_lambda(paren, init)
