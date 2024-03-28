import sys

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_ID,
)
from . import (
    lvgl_ns,
    LVGL_SCHEMA,
    CONF_LVGL_ID,
    add_init_lambda,
    CONF_ANIMATED,
    lv_animated,
    CONF_ARC,
    lv_number_t,
    CONF_WIDGET,
    get_widget,
)
from .lv_validation import requires_component

LVGLNumber = lvgl_ns.class_("LVGLNumber", number.Number)

CONFIG_SCHEMA = cv.All(
    number.number_schema(LVGLNumber)
    .extend(LVGL_SCHEMA)
    .extend(
        {
            cv.Required(CONF_WIDGET): cv.use_id(lv_number_t),
            cv.Optional(CONF_ANIMATED, default=True): lv_animated,
        }
    ),
    requires_component("number"),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await number.register_number(
        var, config, max_value=sys.maxsize, min_value=-sys.maxsize, step=1
    )

    animated = config[CONF_ANIMATED]
    paren = await cg.get_variable(config[CONF_LVGL_ID])
    widget = await get_widget(config[CONF_WIDGET])
    if widget.type_base() == CONF_ARC:
        animated = ""
    else:
        animated = f", {animated}"
    publish = f"{var}->publish_state(lv_{widget.type_base()}_get_value({widget.obj}))"
    init = widget.set_event_cb(publish, "LV_EVENT_VALUE_CHANGED")
    init.extend(
        [
            f"""{var}->set_control_lambda([] (float v) {{
               lv_{widget.type_base()}_set_value({widget.obj}, v{animated});
               lv_event_send({widget.obj}, {paren}->get_custom_change_event(), nullptr);
               {publish};
            }})""",
            f"{var}->traits.set_max_value(lv_{widget.type_base()}_get_max_value({widget.obj}))",
            f"{var}->traits.set_min_value(lv_{widget.type_base()}_get_min_value({widget.obj}))",
            publish,
        ]
    )
    await add_init_lambda(paren, init)
