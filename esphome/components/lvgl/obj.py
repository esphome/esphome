from .defines import CONF_OBJ
from .types import lv_obj_t
from .widget import WidgetType


class ObjType(WidgetType):
    """
    The base LVGL object. All other widgets inherit from this.
    """

    def __init__(self):
        super().__init__(CONF_OBJ, schema={}, modify_schema={})

    @property
    def w_type(self):
        return lv_obj_t

    async def to_code(self, w, config):
        return []


obj_spec = ObjType()
