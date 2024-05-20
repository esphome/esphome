from .defines import CONF_SWITCH
from .types import lv_switch_t
from .widget import WidgetType


class SwitchType(WidgetType):
    def __init__(self):
        super().__init__(CONF_SWITCH)

    @property
    def w_type(self):
        return lv_switch_t

    async def to_code(self, w, config):
        return []


switch_spec = SwitchType()
