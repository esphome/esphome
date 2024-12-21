from esphome import config_validation as cv
from esphome.const import CONF_BUTTON, CONF_ID, CONF_ITEMS, CONF_TEXT
from esphome.core import ID
from esphome.cpp_generator import new_Pvariable, static_const_array
from esphome.cpp_types import nullptr

from ..defines import (
    CONF_BODY,
    CONF_BUTTON_STYLE,
    CONF_BUTTONS,
    CONF_CLOSE_BUTTON,
    CONF_MSGBOXES,
    CONF_TITLE,
    TYPE_FLEX,
    literal,
)
from ..helpers import add_lv_use, lvgl_components_required
from ..lv_validation import lv_bool, lv_pct, lv_text
from ..lvcode import (
    EVENT_ARG,
    LambdaContext,
    LocalVariable,
    lv,
    lv_add,
    lv_assign,
    lv_expr,
    lv_obj,
    lv_Pvariable,
)
from ..schemas import STYLE_SCHEMA, STYLED_TEXT_SCHEMA, container_schema, part_schema
from ..types import LV_EVENT, char_ptr, lv_obj_t
from . import Widget, add_widgets, set_obj_properties
from .button import button_spec
from .buttonmatrix import (
    BUTTONMATRIX_BUTTON_SCHEMA,
    CONF_BUTTON_TEXT_LIST_ID,
    buttonmatrix_spec,
    get_button_data,
    lv_buttonmatrix_t,
    set_btn_data,
)
from .label import CONF_LABEL
from .obj import obj_spec

CONF_MSGBOX = "msgbox"
MSGBOX_SCHEMA = container_schema(
    obj_spec,
    STYLE_SCHEMA.extend(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(lv_obj_t),
            cv.Required(CONF_TITLE): STYLED_TEXT_SCHEMA,
            cv.Optional(CONF_BODY, default=""): STYLED_TEXT_SCHEMA,
            cv.Optional(CONF_BUTTONS): cv.ensure_list(BUTTONMATRIX_BUTTON_SCHEMA),
            cv.Optional(CONF_BUTTON_STYLE): part_schema(buttonmatrix_spec),
            cv.Optional(CONF_CLOSE_BUTTON, default=True): lv_bool,
            cv.GenerateID(CONF_BUTTON_TEXT_LIST_ID): cv.declare_id(char_ptr),
        }
    ),
)


async def msgbox_to_code(top_layer, conf):
    """
    Construct a message box. This consists of a full-screen translucent background enclosing a centered container
    with an optional title, body, close button and a button matrix. And any other widgets the user cares to add
    :param conf: The config data
    :return: code to add to the init lambda
    """
    add_lv_use(
        TYPE_FLEX,
        CONF_BUTTON,
        CONF_LABEL,
        CONF_MSGBOX,
        *buttonmatrix_spec.get_uses(),
        *button_spec.get_uses(),
    )
    lvgl_components_required.add("BUTTONMATRIX")
    messagebox_id = conf[CONF_ID]
    outer_id = f"{messagebox_id.id}_outer"
    outer = lv_Pvariable(lv_obj_t, messagebox_id.id + "_outer")
    buttonmatrix = new_Pvariable(
        ID(
            f"{messagebox_id.id}_buttonmatrix_",
            is_declaration=True,
            type=lv_buttonmatrix_t,
        )
    )
    msgbox = lv_Pvariable(lv_obj_t, messagebox_id.id)
    outer_widget = Widget.create(outer_id, outer, obj_spec, conf)
    outer_widget.move_to_foreground = True
    msgbox_widget = Widget.create(messagebox_id, msgbox, obj_spec, conf)
    msgbox_widget.outer = outer_widget
    buttonmatrix_widget = Widget.create(
        str(buttonmatrix), buttonmatrix, buttonmatrix_spec, conf
    )
    text_list, ctrl_list, width_list, _ = await get_button_data(
        (conf,), buttonmatrix_widget
    )
    text_id = conf[CONF_BUTTON_TEXT_LIST_ID]
    text_list = static_const_array(text_id, text_list)
    text = await lv_text.process(conf[CONF_BODY].get(CONF_TEXT, ""))
    title = await lv_text.process(conf[CONF_TITLE].get(CONF_TEXT, ""))
    close_button = conf[CONF_CLOSE_BUTTON]
    lv_assign(outer, lv_expr.obj_create(top_layer))
    lv_obj.set_width(outer, lv_pct(100))
    lv_obj.set_height(outer, lv_pct(100))
    lv_obj.set_style_bg_opa(outer, 128, 0)
    lv_obj.set_style_bg_color(outer, literal("lv_color_black()"), 0)
    lv_obj.set_style_border_width(outer, 0, 0)
    lv_obj.set_style_pad_all(outer, 0, 0)
    lv_obj.set_style_radius(outer, 0, 0)
    outer_widget.add_flag("LV_OBJ_FLAG_HIDDEN")
    lv_assign(
        msgbox, lv_expr.msgbox_create(outer, title, text, text_list, close_button)
    )
    lv_obj.set_style_align(msgbox, literal("LV_ALIGN_CENTER"), 0)
    lv_add(buttonmatrix.set_obj(lv_expr.msgbox_get_btns(msgbox)))
    if button_style := conf.get(CONF_BUTTON_STYLE):
        button_style = {CONF_ITEMS: button_style}
        await set_obj_properties(buttonmatrix_widget, button_style)
    await set_obj_properties(msgbox_widget, conf)
    await add_widgets(msgbox_widget, conf)
    async with LambdaContext(EVENT_ARG, where=messagebox_id) as close_action:
        outer_widget.add_flag("LV_OBJ_FLAG_HIDDEN")
    if close_button:
        with LocalVariable(
            "close_btn_", lv_obj_t, lv_expr.msgbox_get_close_btn(msgbox)
        ) as close_btn:
            lv_obj.remove_event_cb(close_btn, nullptr)
            lv_obj.add_event_cb(
                close_btn,
                await close_action.get_lambda(),
                LV_EVENT.CLICKED,
                nullptr,
            )
    else:
        lv_obj.add_event_cb(
            outer, await close_action.get_lambda(), LV_EVENT.CLICKED, nullptr
        )

    if len(ctrl_list) != 0 or len(width_list) != 0:
        set_btn_data(buttonmatrix.obj, ctrl_list, width_list)


async def msgboxes_to_code(lv_component, config):
    top_layer = lv.disp_get_layer_top(lv_component.get_disp())
    for conf in config.get(CONF_MSGBOXES, ()):
        await msgbox_to_code(top_layer, conf)
