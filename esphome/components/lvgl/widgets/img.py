from esphome.components.image import INSTANCE_TYPE as IMAGE_TYPE
from esphome.components.mapping import get_mapping_metadata
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
    CONF_IMAGE,
    CONF_MAIN,
    CONF_MAPPING,
    CONF_PIVOT_X,
    CONF_PIVOT_Y,
    CONF_SCALE,
    CONF_SRC,
    CONF_ZOOM,
)
from ..lv_validation import lv_angle, lv_bool, lv_image, scale, size
from ..types import lv_image_t
from . import Widget, WidgetType
from .label import CONF_LABEL

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
            lv_image_t,
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

    def final_validate(self, widget, update_config, widget_config, path):
        src = update_config.get(CONF_SRC)
        if isinstance(src, dict) and CONF_MAPPING in src:
            mapping_id = src[CONF_MAPPING]
            metadata = get_mapping_metadata(mapping_id.id)
            if str(metadata.to_.data_type) != str(IMAGE_TYPE):
                raise cv.Invalid(
                    f"Mapping '{mapping_id}' does not map to an image type, but '{metadata.to_.data_type}'",
                    path=path + [CONF_SRC, CONF_MAPPING],
                )


img_spec = ImgType()
