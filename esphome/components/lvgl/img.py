from esphome.components.image import Image_
import esphome.config_validation as cv
from esphome.const import CONF_ANGLE, CONF_MODE
from esphome.cpp_generator import MockObjClass

from .defines import (
    CONF_ANTIALIAS,
    CONF_IMAGE,
    CONF_OFFSET_X,
    CONF_OFFSET_Y,
    CONF_PIVOT_X,
    CONF_PIVOT_Y,
    CONF_SRC,
    CONF_ZOOM,
    LvConstant,
)
from .helpers import add_lv_use
from .lv_validation import angle, lv_bool, requires_component, size, zoom
from .types import lv_img_t
from .widget import Widget, WidgetType

IMG_SCHEMA = {
    cv.Required(CONF_SRC): cv.All(cv.use_id(Image_), requires_component("image")),
    cv.Optional(CONF_PIVOT_X, default="50%"): size,
    cv.Optional(CONF_PIVOT_Y, default="50%"): size,
    cv.Optional(CONF_ANGLE): angle,
    cv.Optional(CONF_ZOOM): zoom,
    cv.Optional(CONF_OFFSET_X): size,
    cv.Optional(CONF_OFFSET_Y): size,
    cv.Optional(CONF_ANTIALIAS): lv_bool,
    cv.Optional(CONF_MODE): LvConstant("LV_IMG_SIZE_MODE_", "VIRTUAL", "REAL").one_of,
}


class ImgType(WidgetType):
    def __init__(self):
        super().__init__(CONF_IMAGE, IMG_SCHEMA)

    @property
    def w_type(self):
        return lv_img_t

    def get_uses(self):
        return ("img",)

    def obj_creator(self, parent: MockObjClass, config: dict):
        return f"lv_img_create({parent})"

    async def to_code(self, w: Widget, config):
        add_lv_use("label")
        init = []
        if src := config.get(CONF_SRC):
            init.append(f"{w.var}->set_src({src})")
        if cf_angle := config.get(CONF_ANGLE):
            pivot_x = config[CONF_PIVOT_X]
            pivot_y = config[CONF_PIVOT_Y]
            init.append(f"lv_img_set_pivot({w.obj}, {pivot_x}, {pivot_y})")
            init.extend(w.set_property(CONF_ANGLE, cf_angle))
        init.extend(w.set_property(CONF_ZOOM, config))
        init.extend(w.set_property(CONF_OFFSET_X, config))
        init.extend(w.set_property(CONF_OFFSET_Y, config))
        init.extend(w.set_property(CONF_ANTIALIAS, config))
        init.extend(w.set_property(CONF_MODE, config))
        return init


img_spec = ImgType()
