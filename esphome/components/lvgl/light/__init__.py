import esphome.codegen as cg
from esphome.components import light
from esphome.components.light import LightOutput
import esphome.config_validation as cv
from esphome.const import CONF_GAMMA_CORRECT, CONF_LED, CONF_OUTPUT_ID

from ..defines import CONF_LVGL_ID
from ..lvcode import LvContext
from ..schemas import LVGL_SCHEMA
from ..types import LvType, lvgl_ns
from ..widgets import get_widgets, wait_for_widgets

lv_led_t = LvType("lv_led_t")
LVLight = lvgl_ns.class_("LVLight", LightOutput)
CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.Optional(CONF_GAMMA_CORRECT, default=0.0): cv.positive_float,
        cv.Required(CONF_LED): cv.use_id(lv_led_t),
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(LVLight),
    }
).extend(LVGL_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)

    paren = await cg.get_variable(config[CONF_LVGL_ID])
    widget = await get_widgets(config, CONF_LED)
    widget = widget[0]
    await wait_for_widgets()
    async with LvContext(paren) as ctx:
        ctx.add(var.set_obj(widget.obj))
