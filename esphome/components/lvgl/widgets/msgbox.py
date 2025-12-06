from esphome import codegen as cg, config_validation as cv
from esphome.const import CONF_BUTTON, CONF_ID, CONF_TEXT
from esphome.core import ID
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
    LV_OBJ_FLAG,
    TYPE_FLEX,
    add_warning,
    literal,
)
from ..helpers import add_lv_use
from ..lv_validation import lv_bool, lv_color, lv_image, lv_text, pixels_or_percent
from ..lvcode import EVENT_ARG, LambdaContext, LocalVariable, lv, lv_expr, lv_obj
from ..schemas import (
    STYLE_SCHEMA,
    STYLED_TEXT_SCHEMA,
    TEXT_SCHEMA,
    container_schema,
    part_schema,
)
from ..styles import LVStyle
from ..types import LV_EVENT, lv_obj_t
from . import Widget, WidgetType, add_widgets, set_obj_properties, widget_to_code
from .button import button_spec, lv_button_t
from .label import CONF_LABEL
from .obj import obj_spec

CONF_MSGBOX = "msgbox"

OUTER_STYLE = LVStyle(
    "msgbox_outer",
    {
        "bg_opa": 128,
        "bg_color": "black",
        "border_width": 0,
        "pad_all": 0,
        "radius": 0,
    },
)


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

    async def obj_creator(self, parent: MockObjClass, config: dict):
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
            cv.Optional(CONF_BUTTON_STYLE): part_schema(button_spec.parts),
        }
    ),
)


async def msgbox_to_code(top_layer, conf):
    """
    Construct a message box. This consists of a full-screen translucent background enclosing a centered container
    with an optional title, body, close button and a set of footer buttons.
    Header buttons can be added - they can be image buttons only.
    The body of the message box may have any widgets the user wants to add.
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
    if CONF_BUTTON_STYLE in conf:
        add_warning(
            "'button_style' for msgbox is deprecated - style the buttons directly."
        )
    messagebox_id = conf[CONF_ID]
    outer_id = ID(f"{messagebox_id.id}_outer", type=lv_obj_t)
    outer = cg.Pvariable(outer_id, lv_expr.obj_create(top_layer))
    outer_widget = Widget.create(outer_id.id, outer, obj_spec, conf)
    msgbox = cg.Pvariable(messagebox_id, lv_expr.msgbox_create(outer))
    outer_widget.move_to_foreground = True
    msgbox_widget = Widget.create(messagebox_id, msgbox, obj_spec, conf)
    msgbox_widget.outer = outer_widget
    text = await lv_text.process(conf[CONF_BODY].get(CONF_TEXT, ""))
    title = await lv_text.process(conf[CONF_TITLE].get(CONF_TEXT, ""))
    close_button = conf[CONF_CLOSE_BUTTON]
    percent100 = await pixels_or_percent.process(1.0)
    lv_obj.set_size(outer, percent100, percent100)
    outer_widget.add_style(await OUTER_STYLE.get_var())
    outer_widget.add_flag(LV_OBJ_FLAG.HIDDEN)
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
        outer_widget.add_flag(LV_OBJ_FLAG.HIDDEN)
        outer_widget.set_style("bg_color", await lv_color.process("red"))
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
