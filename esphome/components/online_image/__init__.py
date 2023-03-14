import logging
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_DITHER,
    CONF_ESP8266_DISABLE_SSL_SUPPORT,
    CONF_FILE,
    CONF_FORMAT,
    CONF_ID,
    CONF_TYPE,
    CONF_RESIZE,
    CONF_URL,
)
from esphome.core import CORE
from esphome.components import image

DEPENDENCIES = ["network", "display"]
CODEOWNERS = ["@guillempages"]
MULTI_CONF = True

_LOGGER = logging.getLogger(__name__)

online_image_ns = cg.esphome_ns.namespace("online_image")

ImageFormat = online_image_ns.enum("ImageFormat")
IMAGE_FORMAT = {"PNG": ImageFormat.PNG}  # Add new supported formats here

Image_ = online_image_ns.class_("OnlineImage")

CONFIG_SCHEMA = cv.All(
    image.IMAGE_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.declare_id(Image_),
            cv.Optional(CONF_FILE): cv.invalid(
                'Use "url" instead of "file" for online images.'
            ),
            cv.Required(CONF_URL): cv.string,
            cv.Optional(CONF_FORMAT, default="PNG"): cv.enum(IMAGE_FORMAT, upper=True),
            cv.Optional(CONF_BUFFER_SIZE, default=2048): cv.int_range(256, 65536),
        }
    ).extend(cv.polling_component_schema("never"))
)


async def to_code(config):
    url = config[CONF_URL]

    # TODO: Implement dithering in the future
    if config[CONF_DITHER] != "NONE":
        _LOGGER.warning(
            f"Dithering not supported yet. Ignoring dithering request for {config[CONF_ID]}."
        )

    width, height = config.get(CONF_RESIZE, (0, 0))

    var = cg.new_Pvariable(
        config[CONF_ID],
        url,
        width,
        height,
        config[CONF_FORMAT],
        config[CONF_TYPE],
        config[CONF_BUFFER_SIZE],
    )
    await cg.register_component(var, config)
    cg.add_define("USE_ONLINE_IMAGE")

    if CORE.is_esp8266 and not config.get(CONF_ESP8266_DISABLE_SSL_SUPPORT, None):
        cg.add_define("USE_HTTP_REQUEST_ESP8266_HTTPS")

    if CORE.is_esp32:
        cg.add_library("WiFiClientSecure", None)
        cg.add_library("HTTPClient", None)
    if CORE.is_esp8266:
        cg.add_library("ESP8266HTTPClient", None)

    if config[CONF_FORMAT] in ["PNG"]:
        cg.add_define("ONLINE_IMAGE_PNG_SUPPORT")
        cg.add_library("pngle", None)
