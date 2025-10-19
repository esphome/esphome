from esphome.const import CONF_SWITCH

from ..defines import CONF_INDICATOR, CONF_KNOB, CONF_MAIN
from ..schemas import register_lvgl_widget
from ..types import LvBoolean
from . import WidgetType


class SwitchType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_SWITCH,
            LvBoolean("lv_switch_t"),
            (CONF_MAIN, CONF_INDICATOR, CONF_KNOB),
        )

    async def to_code(self, w, config):
        return []


switch_spec = SwitchType()


@register_lvgl_widget(switch_spec)
async def switch_to_code(w, config):
    """Code generation for switch widget - registered via decorator"""
    return await switch_spec.to_code(w, config)
