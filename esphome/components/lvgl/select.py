import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import (
    CONF_ID,
)
from . import (
    lvgl_ns,
    LVGL_SCHEMA,
    CONF_LVGL_ID,
    add_init_lambda,
    CONF_DROPDOWN,
    CONF_WIDGET,
    get_widget,
    lv_select_t,
)
from .lv_validation import requires_component

LVGLSelect = lvgl_ns.class_("LVGLSelect", select.Select)

CONFIG_SCHEMA = cv.All(
    select.select_schema(LVGLSelect)
    .extend(LVGL_SCHEMA)
    .extend(
        {
            cv.Required(CONF_WIDGET): cv.use_id(lv_select_t),
        }
    ),
    requires_component("select"),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await select.register_select(var, config, options=[])
    paren = await cg.get_variable(config[CONF_LVGL_ID])
    init = []
    widget = await get_widget(config[CONF_WIDGET])
    if widget.type_base() == CONF_DROPDOWN:
        animated = ""
    else:
        animated = ", LV_ANIM_OFF"
    publish = (
        f"{var}->publish_index(lv_{widget.type_base()}_get_selected({widget.obj}))"
    )
    init.extend(
        widget.set_event_cb(
            publish,
            "LV_EVENT_VALUE_CHANGED",
        )
    )
    init.extend(
        [
            f"""{var}->set_options(lv_{widget.type_base()}_get_options({widget.obj}));
            {var}->set_control_lambda([] (size_t v) {{
                lv_{widget.type_base()}_set_selected({widget.obj}, v {animated});
               {publish};
            }})""",
            publish,
        ]
    )
    await add_init_lambda(paren, init)
