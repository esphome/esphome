from esphome import config_validation as cv
from esphome.const import CONF_BUTTON, CONF_ID
from esphome.cpp_generator import new_Pvariable, static_const_array
from esphome.cpp_types import nullptr

from ...core import ID
from .btn import btn_spec
from .btnmatrix import (
    BTNM_BTN_SCHEMA,
    CONF_BUTTONMATRIX_TEXT_LIST_ID,
    btnmatrix_spec,
    get_button_data,
    lv_btnmatrix_t,
    set_btn_data,
)
from .defines import (
    CONF_BODY,
    CONF_BUTTONS,
    CONF_CLOSE_BUTTON,
    CONF_MSGBOXES,
    CONF_TEXT,
    CONF_TITLE,
    TYPE_FLEX,
    literal,
)
from .helpers import add_lv_use
from .label import CONF_LABEL
from .lv_validation import lv_bool, lv_pct, lv_text
from .lvcode import (
    LambdaContext,
    LocalVariable,
    lv_add,
    lv_assign,
    lv_expr,
    lv_obj,
    lv_Pvariable,
)
from .obj import obj_spec
from .schemas import STYLE_SCHEMA, STYLED_TEXT_SCHEMA, container_schema
from .styles import TOP_LAYER
from .trigger import lv_event_t_ptr
from .types import char_ptr, lv_obj_t
from .widget import Widget, set_obj_properties

CONF_MSGBOX = "msgbox"
MSGBOX_SCHEMA = container_schema(
    obj_spec,
    STYLE_SCHEMA.extend(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(lv_obj_t),
            cv.Required(CONF_TITLE): STYLED_TEXT_SCHEMA,
            cv.Optional(CONF_BODY): STYLED_TEXT_SCHEMA,
            cv.Optional(CONF_BUTTONS): cv.ensure_list(BTNM_BTN_SCHEMA),
            cv.Optional(CONF_CLOSE_BUTTON): lv_bool,
            cv.GenerateID(CONF_BUTTONMATRIX_TEXT_LIST_ID): cv.declare_id(char_ptr),
        }
    ),
)


async def msgbox_to_code(conf):
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
        *btnmatrix_spec.get_uses(),
        *btn_spec.get_uses(),
    )
    mbid = conf[CONF_ID]
    outer = lv_Pvariable(lv_obj_t, mbid.id)
    btnm = new_Pvariable(
        ID(f"{mbid.id}_btnm_", is_declaration=True, type=lv_btnmatrix_t)
    )
    msgbox = lv_Pvariable(lv_obj_t, f"{mbid.id}_msgbox")
    outer_w = Widget.create(mbid, outer, obj_spec, conf)
    btnm_widg = Widget.create(str(btnm), btnm, btnmatrix_spec, conf)
    text_list, ctrl_list, width_list, _, _ = await get_button_data((conf,), btnm_widg)
    text_id = conf[CONF_BUTTONMATRIX_TEXT_LIST_ID]
    text_list = static_const_array(text_id, text_list)
    if text := conf.get(CONF_BODY):
        text = await lv_text.process(text.get(CONF_TEXT))
    if title := conf.get(CONF_TITLE):
        title = await lv_text.process(title.get(CONF_TEXT))
    close_button = conf[CONF_CLOSE_BUTTON]
    lv_assign(outer, lv_expr.obj_create(TOP_LAYER))
    lv_obj.set_width(outer, lv_pct(100))
    lv_obj.set_height(outer, lv_pct(100))
    lv_obj.set_style_bg_opa(outer, 128, 0)
    lv_obj.set_style_bg_color(outer, literal("lv_color_black()"), 0)
    lv_obj.set_style_border_width(outer, 0, 0)
    lv_obj.set_style_pad_all(outer, 0, 0)
    lv_obj.set_style_radius(outer, 0, 0)
    outer_w.add_flag("LV_OBJ_FLAG_HIDDEN")
    lv_assign(
        msgbox, lv_expr.msgbox_create(outer, title, text, text_list, close_button)
    )
    lv_obj.set_style_align(msgbox, literal("LV_ALIGN_CENTER"), 0)
    lv_add(btnm.set_obj(lv_expr.msgbox_get_btns(msgbox)))
    await set_obj_properties(outer_w, conf)
    if close_button:
        with LambdaContext([(lv_event_t_ptr, "ev")]) as context:
            outer_w.add_flag("LV_OBJ_FLAG_HIDDEN")
        with LocalVariable(
            "close_btn_", lv_obj_t, "*", lv_expr.msgbox_get_close_btn(msgbox)
        ) as close_btn:
            lv_obj.remove_event_cb(close_btn, nullptr)
            lv_obj.add_event_cb(
                close_btn,
                await context.get_lambda(),
                literal("LV_EVENT_CLICKED"),
                nullptr,
            )

    if len(ctrl_list) != 0 or len(width_list) != 0:
        with LocalVariable(
            "msgbox_btns_", lv_obj_t, "*", lv_expr.msgbox_get_btns(msgbox)
        ) as buttons:
            set_btn_data(buttons, ctrl_list, width_list)


async def msgboxes_to_code(config):
    for conf in config.get(CONF_MSGBOXES, ()):
        await msgbox_to_code(conf)
