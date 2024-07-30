from esphome.const import CONF_BUTTON
from esphome.cpp_generator import MockObjClass

from .defines import CONF_MAIN
from .types import LvBoolean, WidgetType


class BtnType(WidgetType):
    def __init__(self):
        super().__init__(CONF_BUTTON, LvBoolean("lv_btn_t"), (CONF_MAIN,))

    async def to_code(self, w, config):
        return []

    def obj_creator(self, parent: MockObjClass, config: dict):
        """
        LVGL 8 calls buttons `btn`
        """
        return f"lv_btn_create({parent})"

    def get_uses(self):
        return ("btn",)


btn_spec = BtnType()
