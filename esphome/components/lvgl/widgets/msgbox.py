from esphome import config_validation as cv
from esphome.components.lvgl.schemas import TEXT_SCHEMA
from esphome.components.lvgl.types import WidgetType
from esphome.components.lvgl.widgets import add_widgets, widget_to_code
from esphome.const import CONF_BUTTON, CONF_ID, CONF_TEXT
from esphome.cpp_generator import MockObjClass
from esphome.cpp_types import nullptr

from ..defines import (
    CONF_BODY,
    CONF_BUTTON_STYLE,
    CONF_BUTTONS,
    CONF_CLOSE_BUTTON,
    CONF_HEADER_BUTTONS,
    CONF_MAIN,
    CONF_MSGBOXES,
    CONF_SRC,
    CONF_TITLE,
    TYPE_FLEX,
    literal,
)
from ..helpers import add_lv_use, lvgl_components_required
from ..lv_validation import lv_bool, lv_image, lv_pct, lv_text
from ..lvcode import (
    EVENT_ARG,
    LambdaContext,
    LocalVariable,
    lv,
    lv_assign,
    lv_expr,
    lv_obj,
    lv_Pvariable,
)
from ..schemas import STYLE_SCHEMA, STYLED_TEXT_SCHEMA, container_schema
from ..types import LV_EVENT, lv_obj_t
from . import Widget, set_obj_properties
from .button import button_spec, lv_button_t
from .label import CONF_LABEL
from .obj import obj_spec

CONF_MSGBOX = "msgbox"


class FooterButtonType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_BUTTON, lv_button_t, (CONF_MAIN,), TEXT_SCHEMA, is_mock=True
        )

    async def obj_creator(self, parent: MockObjClass, config: dict):
        return lv_expr.msgbox_add_footer_button(parent, config[CONF_TEXT])


footer_button_spec = FooterButtonType()


class HeaderButtonType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_BUTTON,
            lv_button_t,
            (CONF_MAIN,),
            cv.Schema(
                {
                    cv.Required(CONF_SRC): lv_image,
                }
            ),
            is_mock=True,
        )

    def obj_creator(self, parent: MockObjClass, config: dict):
        return lv_expr.msgbox_add_header_button(parent, config[CONF_SRC])


header_button_spec = HeaderButtonType()

MSGBOX_SCHEMA = container_schema(
    obj_spec,
    STYLE_SCHEMA.extend(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(lv_obj_t),
            cv.Required(CONF_TITLE): STYLED_TEXT_SCHEMA,
            cv.Optional(CONF_BODY, default=""): STYLED_TEXT_SCHEMA,
            cv.Optional(CONF_BUTTONS): cv.ensure_list(
                container_schema(footer_button_spec)
            ),
            cv.Optional(CONF_HEADER_BUTTONS): cv.ensure_list(
                container_schema(header_button_spec)
            ),
            cv.Optional(CONF_CLOSE_BUTTON, default=True): lv_bool,
            cv.Optional(CONF_BUTTON_STYLE): cv.invalid(
                "button_style is removed - style each button directly"
            ),
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
        *button_spec.get_uses(),
    )
    lvgl_components_required.add("BUTTONMATRIX")
    messagebox_id = conf[CONF_ID]
    outer_id = f"{messagebox_id.id}_outer"
    outer = lv_Pvariable(lv_obj_t, messagebox_id.id + "_outer")
    msgbox = lv_Pvariable(lv_obj_t, messagebox_id.id)
    outer_widget = Widget.create(outer_id, outer, obj_spec, conf)
    outer_widget.move_to_foreground = True
    msgbox_widget = Widget.create(messagebox_id, msgbox, obj_spec, conf)
    msgbox_widget.outer = outer_widget
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
    lv_assign(msgbox, lv_expr.msgbox_create(outer))
    lv.msgbox_add_title(msgbox, title)
    lv.msgbox_add_text(msgbox, text)
    lv_obj.set_style_align(msgbox, literal("LV_ALIGN_CENTER"), 0)
    await set_obj_properties(msgbox_widget, conf)
    await add_widgets(msgbox_widget, conf)
    for button in conf.get(CONF_BUTTONS, ()):
        await widget_to_code(button, footer_button_spec, msgbox)
    for button in conf.get(CONF_HEADER_BUTTONS, ()):
        button[CONF_SRC] = await lv_image.process(button[CONF_SRC])
        await widget_to_code(button, header_button_spec, msgbox)

    async with LambdaContext(EVENT_ARG, where=messagebox_id) as close_action:
        outer_widget.add_flag("LV_OBJ_FLAG_HIDDEN")
    if close_button:
        with LocalVariable(
            "close_btn_", lv_obj_t, lv_expr.msgbox_add_close_button(msgbox)
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


async def msgboxes_to_code(lv_component, config):
    top_layer = lv.disp_get_layer_top(lv_component.get_disp())
    for conf in config.get(CONF_MSGBOXES, ()):
        await msgbox_to_code(top_layer, conf)
