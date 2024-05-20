import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import validate_automation, Trigger
from esphome.const import (
    CONF_NAME,
    CONF_POSITION,
    CONF_SIZE,
    CONF_ON_VALUE,
    CONF_TRIGGER_ID,
    CONF_ID,
    CONF_INDEX,
)
from .codegen import set_obj_properties, add_widgets, action_to_code
from .defines import (
    CONF_TABVIEW,
    CONF_TABS,
    CONF_OBJ,
    DIRECTIONS,
    CONF_ANIMATED,
    CONF_TAB_ID,
    TYPE_FLEX,
    CONF_BTNMATRIX,
)
from .lv_validation import size, lv_int, animated
from .schemas import container_schema
from .types import lv_tab_t, lv_obj_t_ptr, ObjUpdateAction, lv_tabview_t, LvType
from .widget import Widget, get_widget, WidgetType


class TabviewType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_TABVIEW,
            {
                cv.Required(CONF_TABS): cv.ensure_list(
                    container_schema(
                        CONF_OBJ,
                        {
                            cv.Required(CONF_NAME): cv.string,
                            cv.GenerateID(): cv.declare_id(lv_tab_t),
                        },
                    )
                ),
                cv.Optional(CONF_POSITION, default="top"): DIRECTIONS.one_of,
                cv.Optional(CONF_SIZE, default="10%"): size,
                cv.Optional(CONF_ON_VALUE): validate_automation(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                            Trigger.template(lv_obj_t_ptr)
                        )
                    }
                ),
            },
        )

    @property
    def w_type(self):
        return lv_tabview_t

    def get_uses(self):
        return CONF_BTNMATRIX, TYPE_FLEX

    async def to_code(self, w: Widget, config: dict):
        init = []
        for tab_conf in config[CONF_TABS]:
            w_id = tab_conf[CONF_ID]
            tab_obj = cg.Pvariable(w_id, cg.nullptr, type_=lv_tab_t)
            tab_widget = Widget.create(w_id, tab_obj, lv_tab_t)
            init.append(
                f'{tab_obj} = lv_tabview_add_tab({w.obj}, "{tab_conf[CONF_NAME]}")'
            )
            init.extend(await set_obj_properties(tab_widget, tab_conf))
            init.extend(await add_widgets(tab_widget, tab_conf))
        return init

    def obj_creator(self, parent: LvType, config: dict):
        return (
            f"lv_tabview_create({parent}, {config[CONF_POSITION]}, {config[CONF_SIZE]})"
        )


tabview_spec = TabviewType()


@automation.register_action(
    "lvgl.tabview.select",
    ObjUpdateAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(lv_tabview_t),
            cv.Optional(CONF_ANIMATED, default=False): animated,
            cv.Exclusive(CONF_INDEX, CONF_INDEX): lv_int,
            cv.Exclusive(CONF_TAB_ID, CONF_INDEX): cv.use_id(lv_tab_t),
        },
    ).add_extra(cv.has_at_least_one_key(CONF_INDEX, CONF_TAB_ID)),
)
async def tabview_select(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    if tab := config.get(CONF_TAB_ID):
        tab = await cg.get_variable(tab)
        index = f"lv_obj_get_index(lv_tabview_get_content({tab}))"
    else:
        index = config[CONF_INDEX]
    init = [
        f"lv_tabview_set_act({widget.obj}, {index}, {config[CONF_ANIMATED]})",
        f" lv_event_send({widget.obj}, LV_EVENT_VALUE_CHANGED, nullptr)",
    ]
    return await action_to_code(init, action_id, widget, template_arg, args)
