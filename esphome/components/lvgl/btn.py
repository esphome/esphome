from .defines import CONF_BTN
from .types import lv_btn_t
from .widget import WidgetType


class BtnType(WidgetType):
    def __init__(self):
        super().__init__(CONF_BTN)

    @property
    def w_type(self):
        return lv_btn_t

    async def to_code(self, w, config):
        return []


btn_spec = BtnType()
