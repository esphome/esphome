from esphome.cpp_generator import MockObjClass

from .defines import CONF_BUTTON
from .types import lv_btn_t
from .widget import WidgetType


class BtnType(WidgetType):
    def __init__(self):
        super().__init__(CONF_BUTTON)

    @property
    def w_type(self):
        return lv_btn_t

    def obj_creator(self, parent: MockObjClass, config: dict):
        """
        LVGL 8 calls buttons `btn`
        """
        return f"lv_btn_create({parent})"

    def get_uses(self):
        return ("btn",)

    async def to_code(self, w, config):
        return []


btn_spec = BtnType()
