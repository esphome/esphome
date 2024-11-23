import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ANGLE, CONF_MODE

from ..defines import (
    CONF_ANTIALIAS,
    CONF_MAIN,
    CONF_OFFSET_X,
    CONF_OFFSET_Y,
    CONF_PIVOT_X,
    CONF_PIVOT_Y,
    CONF_SRC,
    CONF_ZOOM,
    LvConstant,
)
from ..lv_validation import angle, lv_bool, lv_image, size, zoom
from ..lvcode import lv
from ..types import lv_img_t
from . import Widget, WidgetType
from .label import CONF_LABEL

CONF_IMAGE = "image"

BASE_IMG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PIVOT_X, default="50%"): size,
        cv.Optional(CONF_PIVOT_Y, default="50%"): size,
        cv.Optional(CONF_ANGLE): angle,
        cv.Optional(CONF_ZOOM): zoom,
        cv.Optional(CONF_OFFSET_X): size,
        cv.Optional(CONF_OFFSET_Y): size,
        cv.Optional(CONF_ANTIALIAS): lv_bool,
        cv.Optional(CONF_MODE): LvConstant(
            "LV_IMG_SIZE_MODE_", "VIRTUAL", "REAL"
        ).one_of,
    }
)

IMG_SCHEMA = BASE_IMG_SCHEMA.extend(
    {
        cv.Required(CONF_SRC): lv_image,
    }
)

IMG_MODIFY_SCHEMA = BASE_IMG_SCHEMA.extend(
    {
        cv.Optional(CONF_SRC): lv_image,
    }
)


class ImgType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_IMAGE,
            lv_img_t,
            (CONF_MAIN,),
            IMG_SCHEMA,
            IMG_MODIFY_SCHEMA,
            lv_name="img",
        )

    def get_uses(self):
        return "img", CONF_LABEL

    async def to_code(self, w: Widget, config):
        if src := config.get(CONF_SRC):
            src = await cg.get_variable(src)
            lv.img_set_src(w.obj, await lv_image.process(src))
        if (cf_angle := config.get(CONF_ANGLE)) is not None:
            pivot_x = config[CONF_PIVOT_X]
            pivot_y = config[CONF_PIVOT_Y]
            lv.img_set_pivot(w.obj, pivot_x, pivot_y)
            lv.img_set_angle(w.obj, cf_angle)
        if (img_zoom := config.get(CONF_ZOOM)) is not None:
            lv.img_set_zoom(w.obj, img_zoom)
        if (offset := config.get(CONF_OFFSET_X)) is not None:
            lv.img_set_offset_x(w.obj, offset)
        if (offset := config.get(CONF_OFFSET_Y)) is not None:
            lv.img_set_offset_y(w.obj, offset)
        if CONF_ANTIALIAS in config:
            lv.img_set_antialias(w.obj, config[CONF_ANTIALIAS])
        if mode := config.get(CONF_MODE):
            lv.img_set_mode(w.obj, mode)


img_spec = ImgType()
