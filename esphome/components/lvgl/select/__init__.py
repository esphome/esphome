from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_OPTIONS

from ..defines import CONF_ANIMATED, CONF_WIDGET, literal
from ..lvcode import LvContext
from ..types import LvSelect, lvgl_ns
from ..widgets import get_widgets, wait_for_widgets

LVGLSelect = lvgl_ns.class_("LVGLSelect", select.Select)

CONFIG_SCHEMA = select.select_schema(LVGLSelect).extend(
    {
        cv.Required(CONF_WIDGET): cv.use_id(LvSelect),
        cv.Optional(CONF_ANIMATED, default=False): cv.boolean,
    }
)


async def to_code(config):
    widget = await get_widgets(config, CONF_WIDGET)
    widget = widget[0]
    options = widget.config.get(CONF_OPTIONS, [])
    selector = await select.new_select(config, options=options)
    await wait_for_widgets()
    async with LvContext() as ctx:
        ctx.add(
            selector.set_widget(
                widget.var,
                literal("LV_ANIM_ON" if config[CONF_ANIMATED] else "LV_ANIM_OFF"),
            )
        )
