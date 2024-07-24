from .defines import CONF_MAIN, CONF_OBJ
from .types import WidgetType, lv_obj_t


class ObjType(WidgetType):
    """
    The base LVGL object. All other widgets inherit from this.
    """

    def __init__(self):
        super().__init__(CONF_OBJ, lv_obj_t, (CONF_MAIN,), schema={}, modify_schema={})

    async def to_code(self, w, config):
        return []


obj_spec = ObjType()
