import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import LVGL_SCHEMA, add_init_lambda, lv_validation as lv
from ..defines import CONF_ANIMATED, CONF_LVGL_ID, CONF_WIDGET
from ..lv_validation import requires_component
from ..types import LvNumber, lvgl_ns
from ..widget import get_widget

LVGLNumber = lvgl_ns.class_("LVGLNumber", number.Number)

CONFIG_SCHEMA = cv.All(
    number.number_schema(LVGLNumber)
    .extend(LVGL_SCHEMA)
    .extend(
        {
            cv.Required(CONF_WIDGET): cv.use_id(LvNumber),
            cv.Optional(CONF_ANIMATED, default=True): lv.animated,
        }
    ),
    requires_component("number"),
)


async def to_code(config):
    animated = config[CONF_ANIMATED]
    paren = await cg.get_variable(config[CONF_LVGL_ID])
    widget = await get_widget(config[CONF_WIDGET])
    value = widget.get_value()
    var = cg.new_Pvariable(config[CONF_ID])
    await number.register_number(
        var,
        config,
        max_value=widget.range_to,
        min_value=widget.range_from,
        step=widget.step,
    )

    publish = f"{var}->publish_state({value})"
    init = widget.set_event_cb(publish, "LV_EVENT_VALUE_CHANGED")
    init.append(f"{var}->set_control_lambda([] (float v) {{")
    init.extend(widget.set_value("v", animated))
    init.extend(
        [
            f"""
               lv_event_send({widget.obj}, {paren}->get_custom_change_event(), nullptr);
               {publish};
            }})""",
            f"""{var}->traits.set_max_value({widget.get_mxx_value("max")})""",
            f"""{var}->traits.set_min_value({widget.get_mxx_value("min")})""",
            publish,
        ]
    )
    await add_init_lambda(paren, init)
