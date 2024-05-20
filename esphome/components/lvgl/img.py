import esphome.config_validation as cv
from esphome.components.image import Image_
from esphome.const import CONF_MODE, CONF_ANGLE
from .defines import (
    CONF_IMG,
    CONF_SRC,
    CONF_PIVOT_X,
    CONF_OFFSET_X,
    CONF_OFFSET_Y,
    CONF_ANTIALIAS,
    LvConstant,
    CONF_PIVOT_Y,
    CONF_ZOOM,
)
from .lv_validation import size, angle, zoom, lv_bool
from .types import lv_img_t
from .widget import Widget, WidgetType

IMG_SCHEMA = {
    cv.Required(CONF_SRC): cv.use_id(Image_),
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
        super().__init__(CONF_IMG, IMG_SCHEMA)

    @property
    def w_type(self):
        return lv_img_t

    async def to_code(self, w: Widget, config):
        init = []
        if src := config.get(CONF_SRC):
            init.extend(w.set_property(CONF_SRC, f"lv_img_from({src})"))
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
