from .defines import CONF_BTN, CONF_MAIN
from .types import LvBoolean, WidgetType


class BtnType(WidgetType):
    def __init__(self):
        super().__init__(CONF_BTN, LvBoolean("lv_btn_t"), (CONF_MAIN,))

    async def to_code(self, w, config):
        return []


btn_spec = BtnType()
