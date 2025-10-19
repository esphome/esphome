from esphome.const import CONF_BUTTON

from ..defines import CONF_MAIN
from ..schemas import register_lvgl_widget
from ..types import LvBoolean, WidgetType

lv_button_t = LvBoolean("lv_btn_t")


class ButtonType(WidgetType):
    def __init__(self):
        super().__init__(CONF_BUTTON, lv_button_t, (CONF_MAIN,), lv_name="btn")

    def get_uses(self):
        return ("btn",)

    async def to_code(self, w, config):
        return []


button_spec = ButtonType()


@register_lvgl_widget(button_spec)
async def button_to_code(w, config):
    """Code generation for button widget - registered via decorator"""
    return await button_spec.to_code(w, config)
