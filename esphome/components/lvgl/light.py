import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.light import LightOutput
from esphome.components import light
from esphome.const import (
    CONF_LED,
    CONF_OUTPUT_ID,
    CONF_GAMMA_CORRECT,
)
from . import (
    add_init_lambda,
    LVGL_SCHEMA,
    CONF_LVGL_ID,
    lv_led_t,
    lvgl_ns,
    get_widget,
)

from .lv_validation import requires_component

LVLight = lvgl_ns.class_("LVLight", LightOutput)
CONFIG_SCHEMA = cv.All(
    light.RGB_LIGHT_SCHEMA.extend(
        {
            cv.Optional(CONF_GAMMA_CORRECT, default=0.0): cv.positive_float,
            cv.Required(CONF_LED): cv.use_id(lv_led_t),
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(LVLight),
        }
    ).extend(LVGL_SCHEMA),
    requires_component("light"),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    paren = await cg.get_variable(config[CONF_LVGL_ID])
    widget = await get_widget(config[CONF_LED])
    await add_init_lambda(
        paren,
        [
            f"""{var}->set_control_lambda([] (lv_color_t v) {{
                lv_led_set_color({widget.obj}, v);
                lv_led_on({widget.obj});
            }})"""
        ],
    )
