from esphome.const import CONF_BUTTON

from .defines import CONF_MAIN
from .types import LvBoolean, WidgetType

lv_btn_t = LvBoolean("lv_btn_t")


class BtnType(WidgetType):
    def __init__(self):
        super().__init__(CONF_BUTTON, lv_btn_t, (CONF_MAIN,), lv_name="btn")

    def get_uses(self):
        return ("btn",)

    async def to_code(self, w, config):
        return []


btn_spec = BtnType()
