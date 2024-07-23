import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import LVGL_SCHEMA, add_init_lambda
from ..defines import CONF_ANIMATED, CONF_LVGL_ID, CONF_WIDGET
from ..lv_validation import requires_component
from ..types import LvSelect, lvgl_ns
from ..widget import get_widget

LVGLSelect = lvgl_ns.class_("LVGLSelect", select.Select)

CONFIG_SCHEMA = cv.All(
    select.select_schema(LVGLSelect)
    .extend(LVGL_SCHEMA)
    .extend(
        {
            cv.Required(CONF_WIDGET): cv.use_id(LvSelect),
            cv.Optional(CONF_ANIMATED, default=False): cv.boolean,
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
    publish = f"""{var}->publish_index({widget.get_property("selected")[0]})"""
    init.extend(
        widget.set_event_cb(
            publish,
            "LV_EVENT_VALUE_CHANGED",
        )
    )
    init.append(
        f"""{var}->set_options({widget.get_property("options")[0]});
            {var}->set_control_lambda([] (size_t v) {{
            """
    )
    init.extend(widget.set_property("selected", "v", animated=config[CONF_ANIMATED]))
    init.extend([publish, "})", publish])
    await add_init_lambda(paren, init)
