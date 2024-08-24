from ..defines import CONF_INDICATOR, CONF_KNOB, CONF_MAIN
from ..types import LvBoolean
from . import WidgetType

CONF_SWITCH = "switch"


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
