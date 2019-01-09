# coding=utf-8
import logging

import voluptuous as vol

from esphomeyaml import core
from esphomeyaml.components import display, font
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_FILE, CONF_ID, CONF_RESIZE
from esphomeyaml.core import CORE, HexInt
from esphomeyaml.cpp_generator import ArrayInitializer, MockObj, Pvariable, RawExpression, add
from esphomeyaml.cpp_types import App

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ['display']
MULTI_CONF = True

Image_ = display.display_ns.class_('Image')

CONF_RAW_DATA_ID = 'raw_data_id'

IMAGE_SCHEMA = vol.Schema({
    vol.Required(CONF_ID): cv.declare_variable_id(Image_),
    vol.Required(CONF_FILE): cv.file_,
    vol.Optional(CONF_RESIZE): cv.dimensions,
    cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_variable_id(None),
})

CONFIG_SCHEMA = vol.All(font.validate_pillow_installed, IMAGE_SCHEMA)


def to_code(config):
    from PIL import Image

    path = CORE.relative_path(config[CONF_FILE])
    try:
        image = Image.open(path)
    except Exception as e:
        raise core.EsphomeyamlError(u"Could not load image file {}: {}".format(path, e))

    if CONF_RESIZE in config:
        image.thumbnail(config[CONF_RESIZE])

    image = image.convert('1', dither=Image.NONE)
    width, height = image.size
    if width > 500 or height > 500:
        _LOGGER.warning("The image you requested is very big. Please consider using the resize "
                        "parameter")
    width8 = ((width + 7) // 8) * 8
    data = [0 for _ in range(height * width8 // 8)]
    for y in range(height):
        for x in range(width):
            if image.getpixel((x, y)):
                continue
            pos = x + y * width8
            data[pos // 8] |= 0x80 >> (pos % 8)

    raw_data = MockObj(config[CONF_RAW_DATA_ID])
    add(RawExpression('static const uint8_t {}[{}] PROGMEM = {}'.format(
        raw_data, len(data),
        ArrayInitializer(*[HexInt(x) for x in data], multiline=False))))

    rhs = App.make_image(raw_data, width, height)
    Pvariable(config[CONF_ID], rhs)
