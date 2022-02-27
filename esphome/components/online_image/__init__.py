import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_ID,
    CONF_FORMAT,
    CONF_RAW_DATA_ID,
    CONF_URL,
)
from esphome.core import CORE

DEPENDENCIES = ["display"]
MULTI_CONF = True

online_image_ns = cg.esphome_ns.namespace("online_image")

ImageFormat = online_image_ns.enum("ImageFormat")
IMAGE_FORMAT = {
#    "JPEG": ImageFormat.JPEG,  # Not yet supported
    "PNG": ImageFormat.PNG,
}

Image_ = online_image_ns.class_("OnlineImage")

IMAGE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(Image_),
        cv.Required(CONF_URL): cv.string,
        cv.Optional(CONF_FORMAT, default="PNG"): cv.enum(IMAGE_FORMAT, upper=True),
        cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
    }
)

CONFIG_SCHEMA = IMAGE_SCHEMA

async def to_code(config):
    url = config[CONF_URL]
    width, height = 0, 0

    cg.new_Pvariable(
        config[CONF_ID], url, width, height, config[CONF_FORMAT]
    )
    cg.add_define("USE_ONLINE_IMAGE")
    if CORE.is_esp32:
        cg.add_library("WiFiClientSecure", None)
        cg.add_library("HTTPClient", None)
    if CORE.is_esp8266:
        cg.add_library("ESP8266HTTPClient", None)

    if config[CONF_FORMAT] in ['PNG']:
        cg.add_define("ONLINE_IMAGE_PNG_SUPPORT")
        cg.add_library("pngle", None)
