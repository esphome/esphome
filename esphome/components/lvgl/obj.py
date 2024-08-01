from esphome import automation

from .automation import update_to_code
from .defines import CONF_MAIN, CONF_OBJ
from .schemas import create_modify_schema
from .types import ObjUpdateAction, WidgetType, lv_obj_t


class ObjType(WidgetType):
    """
    The base LVGL object. All other widgets inherit from this.
    """

    def __init__(self):
        super().__init__(CONF_OBJ, lv_obj_t, (CONF_MAIN,), schema={}, modify_schema={})

    async def to_code(self, w, config):
        return []


obj_spec = ObjType()


@automation.register_action(
    "lvgl.widget.update", ObjUpdateAction, create_modify_schema(obj_spec)
)
async def obj_update_to_code(config, action_id, template_arg, args):
    return await update_to_code(config, action_id, template_arg, args)
