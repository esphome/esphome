import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_SIZE, CONF_TEXT

from ..defines import CONF_MAIN
from ..lv_validation import color, lv_color, lv_int, lv_text
from ..lvcode import LocalVariable, lv
from ..schemas import TEXT_SCHEMA
from ..types import lv_obj_t
from . import Widget, WidgetType
from .img import CONF_IMAGE

CONF_QRCODE = "qrcode"
CONF_DARK_COLOR = "dark_color"
CONF_LIGHT_COLOR = "light_color"

QRCODE_SCHEMA = {
    **TEXT_SCHEMA,
    cv.Optional(CONF_DARK_COLOR, default="black"): color,
    cv.Optional(CONF_LIGHT_COLOR, default="white"): color,
    cv.Required(CONF_SIZE): lv_int,
}

QRCODE_MODIFY_SCHEMA = {
    **TEXT_SCHEMA,
    cv.Optional(CONF_DARK_COLOR): lv_color,
    cv.Optional(CONF_LIGHT_COLOR): lv_color,
    cv.Optional(CONF_SIZE): lv_int,
}


class QrCodeType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_QRCODE,
            lv_obj_t,
            (CONF_MAIN,),
            QRCODE_SCHEMA,
            modify_schema=QRCODE_MODIFY_SCHEMA,
        )

    def get_uses(self):
        return "canvas", CONF_IMAGE

    async def to_code(self, w: Widget, config):
        await w.set_property(
            CONF_LIGHT_COLOR, await lv_color.process(config.get(CONF_LIGHT_COLOR))
        )
        await w.set_property(
            CONF_DARK_COLOR, await lv_color.process(config.get(CONF_DARK_COLOR))
        )
        await w.set_property(CONF_SIZE, await lv_int.process(config.get(CONF_SIZE)))
        if (value := config.get(CONF_TEXT)) is not None:
            if isinstance(value, str):
                lv.qrcode_update(w.obj, value, len(value))
                return
            value = await lv_text.process(value)
            with LocalVariable("qr_text", cg.std_string, value, modifier="") as str_obj:
                lv.qrcode_update(w.obj, str_obj.c_str(), str_obj.size())


qr_code_spec = QrCodeType()
