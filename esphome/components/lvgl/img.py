import esphome.config_validation as cv
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
from ..image import Image_
from ...const import CONF_MODE, CONF_ANGLE

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
            init.append(f"lv_img_set_src({w.obj}, lv_img_from({src}))")
        if cf_angle := config.get(CONF_ANGLE):
            pivot_x = config[CONF_PIVOT_X]
            pivot_y = config[CONF_PIVOT_Y]
            init.extend(
                [
                    f"lv_img_set_pivot({w.obj}, {pivot_x}, {pivot_y})",
                    f"lv_img_set_angle({w.obj}, {cf_angle})",
                ]
            )
        if cf_zoom := config.get(CONF_ZOOM):
            init.append(f"lv_img_set_zoom({w.obj}, {cf_zoom})")
        if offset_x := config.get(CONF_OFFSET_X):
            init.append(f"lv_img_set_offset_x({w.obj}, {offset_x})")
        if offset_y := config.get(CONF_OFFSET_Y):
            init.append(f"lv_img_set_offset_y({w.obj}, {offset_y})")
        if antialias := config.get(CONF_ANTIALIAS):
            init.append(f"lv_img_set_antialias({w.obj}, {antialias})")
        if mode := config.get(CONF_MODE):
            init.append(f"lv_img_set_size_mode({w.obj}, {mode})")
        return init


img_spec = ImgType()
