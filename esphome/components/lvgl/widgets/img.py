import esphome.config_validation as cv
from esphome.const import (
    CONF_ANGLE,
    CONF_MODE,
    CONF_OFFSET_X,
    CONF_OFFSET_Y,
    CONF_ROTATION,
)

from ..defines import (
    CONF_ANTIALIAS,
    CONF_MAIN,
    CONF_PIVOT_X,
    CONF_PIVOT_Y,
    CONF_SCALE,
    CONF_SRC,
    CONF_ZOOM,
)
from ..lv_validation import lv_angle, lv_bool, lv_image, scale, size
from ..types import lv_img_t
from . import Widget, WidgetType
from .label import CONF_LABEL

CONF_IMAGE = "image"

BASE_IMG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PIVOT_X): size,
        cv.Optional(CONF_PIVOT_Y): size,
        cv.Exclusive(CONF_ANGLE, CONF_ROTATION): lv_angle,
        cv.Exclusive(CONF_ROTATION, CONF_ROTATION): lv_angle,
        cv.Exclusive(CONF_ZOOM, CONF_SCALE): scale,
        cv.Exclusive(CONF_SCALE, CONF_SCALE): scale,
        cv.Optional(CONF_OFFSET_X): size,
        cv.Optional(CONF_OFFSET_Y): size,
        cv.Optional(CONF_ANTIALIAS): lv_bool,
        cv.Optional(CONF_MODE): cv.invalid(f"{CONF_MODE} is not supported in LVGL 9.x"),
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
        )

    def get_uses(self):
        return CONF_IMAGE, CONF_LABEL

    async def to_code(self, w: Widget, config):
        await w.set_property(CONF_SRC, await lv_image.process(config.get(CONF_SRC)))
        for prop, validator in BASE_IMG_SCHEMA.schema.items():
            await w.set_property(prop, config, processor=validator)


img_spec = ImgType()
