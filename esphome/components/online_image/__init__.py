import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_ESP8266_DISABLE_SSL_SUPPORT,
    CONF_ID,
    CONF_FORMAT,
    CONF_RAW_DATA_ID,
    CONF_URL,
)
from esphome.core import CORE

DEPENDENCIES = ["display"]
CODEOWNERS = ["@guillempages"]
MULTI_CONF = True

CONF_SLOW_DRAWING = "slow_drawing"

online_image_ns = cg.esphome_ns.namespace("online_image")

ImageFormat = online_image_ns.enum("ImageFormat")
IMAGE_FORMAT = {"PNG": ImageFormat.PNG}  # Add new supported formats here

Image_ = online_image_ns.class_("OnlineImage")

IMAGE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(Image_),
        cv.Required(CONF_URL): cv.string,
        cv.Optional(CONF_FORMAT, default="PNG"): cv.enum(IMAGE_FORMAT, upper=True),
        cv.Optional(CONF_BUFFER_SIZE, default=2048): cv.int_range(256, 65536),
        cv.Optional(CONF_SLOW_DRAWING, default=False): cv.boolean,
        cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
    }
)

CONFIG_SCHEMA = IMAGE_SCHEMA


async def to_code(config):
    url = config[CONF_URL]
    width, height = 0, 0

    cg.new_Pvariable(
        config[CONF_ID],
        url,
        width,
        height,
        config[CONF_FORMAT],
        config[CONF_BUFFER_SIZE],
    )
    cg.add_define("USE_ONLINE_IMAGE")

    if CORE.is_esp8266 and not config[CONF_ESP8266_DISABLE_SSL_SUPPORT]:
        cg.add_define("USE_HTTP_REQUEST_ESP8266_HTTPS")

    if CORE.is_esp32:
        cg.add_library("WiFiClientSecure", None)
        cg.add_library("HTTPClient", None)
    if CORE.is_esp8266:
        cg.add_library("ESP8266HTTPClient", None)

    if config[CONF_FORMAT] in ["PNG"]:
        cg.add_define("ONLINE_IMAGE_PNG_SUPPORT")
        cg.add_library("pngle", None)

    if config[CONF_SLOW_DRAWING]:
        cg.add_define("ONLINE_IMAGE_WATCHDOG_ON_DECODE")
