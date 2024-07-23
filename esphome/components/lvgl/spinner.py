import esphome.config_validation as cv

from .defines import CONF_ARC, CONF_ARC_LENGTH, CONF_SPIN_TIME, CONF_SPINNER
from .lv_validation import angle
from .types import LvType, lv_spinner_t
from .widget import Widget, WidgetType

SPINNER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ARC_LENGTH): angle,
        cv.Required(CONF_SPIN_TIME): cv.positive_time_period_milliseconds,
    }
)


class SpinnerType(WidgetType):
    def __init__(self):
        super().__init__(CONF_SPINNER, SPINNER_SCHEMA, {})

    @property
    def w_type(self):
        return lv_spinner_t

    async def to_code(self, w: Widget, config):
        return []

    def get_uses(self):
        return (CONF_ARC,)

    def obj_creator(self, parent: LvType, config: dict):
        spin_time = config[CONF_SPIN_TIME].total_milliseconds
        arc_length = config[CONF_ARC_LENGTH] // 10
        return f"lv_spinner_create({parent}, {spin_time}, {arc_length})"


spinner_spec = SpinnerType()
