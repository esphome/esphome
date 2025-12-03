from esphome import config_validation as cv
from esphome.const import CONF_BUTTON, CONF_TEXT
from esphome.cpp_generator import MockObj

from ..defines import CONF_MAIN, CONF_WIDGETS
from ..helpers import add_lv_use
from ..lv_validation import lv_text
from ..lvcode import lv, lv_expr
from ..schemas import TEXT_SCHEMA
from ..types import LvBoolean, WidgetType
from . import Widget
from .label import label_spec

lv_button_t = LvBoolean("lv_btn_t")


class ButtonType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_BUTTON, lv_button_t, (CONF_MAIN,), schema=TEXT_SCHEMA, lv_name="btn"
        )

    def validate(self, value):
        if CONF_TEXT in value:
            if CONF_WIDGETS in value:
                raise cv.Invalid("Cannot use both text and widgets in a button")
            add_lv_use("label")
        return value

    def get_uses(self):
        return ("btn",)

    def on_create(self, var: MockObj, config: dict):
        if CONF_TEXT in config:
            lv.label_create(var)
        return var

    async def to_code(self, w: Widget, config):
        if text := config.get(CONF_TEXT):
            label_widget = Widget.create(
                None, lv_expr.obj_get_child(w.obj, 0), label_spec
            )
            await label_widget.set_property(CONF_TEXT, await lv_text.process(text))

    def final_validate(self, widget, update_config, widget_config, path):
        if CONF_TEXT in update_config and CONF_TEXT not in widget_config:
            raise cv.Invalid(
                "Button must have 'text:' configured to allow updating text", path
            )


button_spec = ButtonType()
