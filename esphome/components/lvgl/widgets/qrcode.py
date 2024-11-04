import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_SIZE, CONF_TEXT
from esphome.cpp_generator import MockObjClass

from ..defines import CONF_MAIN, literal
from ..lv_validation import color, color_retmapper, lv_text
from ..lvcode import LocalVariable, lv, lv_expr
from ..schemas import TEXT_SCHEMA
from ..types import WidgetType, lv_obj_t
from . import Widget

CONF_QRCODE = "qrcode"
CONF_DARK_COLOR = "dark_color"
CONF_LIGHT_COLOR = "light_color"

QRCODE_SCHEMA = TEXT_SCHEMA.extend(
    {
        cv.Optional(CONF_DARK_COLOR, default="black"): color,
        cv.Optional(CONF_LIGHT_COLOR, default="white"): color,
        cv.Required(CONF_SIZE): cv.int_,
    }
)


class QrCodeType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_QRCODE,
            lv_obj_t,
            (CONF_MAIN,),
            QRCODE_SCHEMA,
            modify_schema=TEXT_SCHEMA,
        )

    def get_uses(self):
        return ("canvas", "img")

    def obj_creator(self, parent: MockObjClass, config: dict):
        dark_color = color_retmapper(config[CONF_DARK_COLOR])
        light_color = color_retmapper(config[CONF_LIGHT_COLOR])
        size = config[CONF_SIZE]
        return lv_expr.call("qrcode_create", parent, size, dark_color, light_color)

    async def to_code(self, w: Widget, config):
        if (value := config.get(CONF_TEXT)) is not None:
            value = await lv_text.process(value)
            with LocalVariable(
                "qr_text", cg.const_char_ptr, value, modifier=""
            ) as str_obj:
                lv.qrcode_update(w.obj, str_obj, literal(f"strlen({str_obj})"))


qr_code_spec = QrCodeType()
