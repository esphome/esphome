from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INDEX, CONF_NAME, CONF_POSITION, CONF_SIZE
from esphome.cpp_generator import MockObjClass

from ..automation import action_to_code
from ..defines import (
    CONF_ANIMATED,
    CONF_MAIN,
    CONF_TAB_ID,
    CONF_TABS,
    DIRECTIONS,
    TYPE_FLEX,
    literal,
)
from ..lv_validation import animated, lv_int, size
from ..lvcode import LocalVariable, lv, lv_assign, lv_expr
from ..schemas import container_schema, part_schema
from ..types import LV_EVENT, LvType, ObjUpdateAction, lv_obj_t, lv_obj_t_ptr
from . import Widget, WidgetType, add_widgets, get_widgets, set_obj_properties
from .buttonmatrix import buttonmatrix_spec
from .obj import obj_spec

CONF_TABVIEW = "tabview"
CONF_TAB_STYLE = "tab_style"

lv_tab_t = LvType("lv_obj_t")

TABVIEW_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TABS): cv.ensure_list(
            container_schema(
                obj_spec,
                {
                    cv.Required(CONF_NAME): cv.string,
                    cv.GenerateID(): cv.declare_id(lv_tab_t),
                },
            )
        ),
        cv.Optional(CONF_TAB_STYLE): part_schema(buttonmatrix_spec.parts),
        cv.Optional(CONF_POSITION, default="top"): DIRECTIONS.one_of,
        cv.Optional(CONF_SIZE, default="10%"): size,
    }
)


class TabviewType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_TABVIEW,
            LvType(
                "lv_tabview_t",
                largs=[(lv_obj_t_ptr, "tab")],
                lvalue=lambda w: lv_expr.obj_get_child(
                    lv_expr.tabview_get_content(w.obj),
                    lv_expr.tabview_get_tab_act(w.obj),
                ),
                has_on_value=True,
            ),
            parts=(CONF_MAIN,),
            schema=TABVIEW_SCHEMA,
            modify_schema={},
        )

    def get_uses(self):
        return "btnmatrix", TYPE_FLEX

    async def to_code(self, w: Widget, config: dict):
        for tab_conf in config[CONF_TABS]:
            w_id = tab_conf[CONF_ID]
            tab_obj = cg.Pvariable(w_id, cg.nullptr, type_=lv_tab_t)
            tab_widget = Widget.create(w_id, tab_obj, obj_spec)
            lv_assign(tab_obj, lv_expr.tabview_add_tab(w.obj, tab_conf[CONF_NAME]))
            await set_obj_properties(tab_widget, tab_conf)
            await add_widgets(tab_widget, tab_conf)
        if button_style := config.get(CONF_TAB_STYLE):
            with LocalVariable(
                "tabview_btnmatrix", lv_obj_t, rhs=lv_expr.tabview_get_tab_btns(w.obj)
            ) as btnmatrix_obj:
                await set_obj_properties(Widget(btnmatrix_obj, obj_spec), button_style)

    def obj_creator(self, parent: MockObjClass, config: dict):
        return lv_expr.call(
            "tabview_create",
            parent,
            literal(config[CONF_POSITION]),
            literal(config[CONF_SIZE]),
        )


tabview_spec = TabviewType()


@automation.register_action(
    "lvgl.tabview.select",
    ObjUpdateAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(tabview_spec.w_type),
            cv.Optional(CONF_ANIMATED, default=False): animated,
            cv.Required(CONF_INDEX): lv_int,
        },
    ).add_extra(cv.has_at_least_one_key(CONF_INDEX, CONF_TAB_ID)),
)
async def tabview_select(config, action_id, template_arg, args):
    widget = await get_widgets(config)
    index = config[CONF_INDEX]

    async def do_select(w: Widget):
        lv.tabview_set_act(w.obj, index, literal(config[CONF_ANIMATED]))
        lv.event_send(w.obj, LV_EVENT.VALUE_CHANGED, cg.nullptr)

    return await action_to_code(widget, do_select, action_id, template_arg, args)
