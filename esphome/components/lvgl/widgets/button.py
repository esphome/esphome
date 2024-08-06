from esphome.const import CONF_BUTTON

from ..defines import CONF_MAIN
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
