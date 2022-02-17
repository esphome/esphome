import logging

from esphome.components import display
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_DITHER,
    CONF_ID,
    CONF_FORMAT,
    CONF_RAW_DATA_ID,
    CONF_RESIZE,
    CONF_TYPE,
    CONF_URL,
)

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["display"]
MULTI_CONF = True

online_image_ns = cg.esphome_ns.namespace("online_image")

ImageFormat = online_image_ns.enum("ImageFormat")
IMAGE_FORMAT = {
#    "JPEG": ImageFormat.JPEG,
    "PNG": ImageFormat.PNG,
}

Image_ = online_image_ns.class_("OnlineImage")

IMAGE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(Image_),
        cv.Required(CONF_URL): cv.string,
        cv.Optional(CONF_RESIZE): cv.dimensions,
        cv.Optional(CONF_FORMAT, default="PNG"): cv.enum(IMAGE_FORMAT, upper=True),
        cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
    }
)

CONFIG_SCHEMA = IMAGE_SCHEMA

async def to_code(config):
    url = config[CONF_URL]
    width, height = 0, 0
    if config.get(CONF_RESIZE):
        width, height = config[CONF_RESIZE]

    cg.new_Pvariable(
        config[CONF_ID], url, width, height
    )
    cg.add_define("USE_ONLINE_IMAGE")
    cg.add_library("pngle", None)
