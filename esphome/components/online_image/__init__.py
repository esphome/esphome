import logging
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_ESP8266_DISABLE_SSL_SUPPORT,
    CONF_FORMAT,
    CONF_ID,
    CONF_TYPE,
    CONF_RESIZE,
    CONF_URL,
)
from esphome.core import CORE
from esphome.components.image import (
    Image_,
    IMAGE_TYPE,
    CONF_USE_TRANSPARENCY,
    validate_cross_dependencies,
)

DEPENDENCIES = ["network", "display"]
CODEOWNERS = ["@guillempages"]
MULTI_CONF = True

_LOGGER = logging.getLogger(__name__)

online_image_ns = cg.esphome_ns.namespace("online_image")

ImageFormat = online_image_ns.enum("ImageFormat")
IMAGE_FORMAT = {"PNG": ImageFormat.PNG}  # Add new supported formats here

OnlineImage_ = online_image_ns.class_("OnlineImage", cg.PollingComponent, Image_)

ONLINE_IMAGE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(OnlineImage_),
        cv.Required(CONF_URL): cv.string,
        cv.Optional(CONF_RESIZE): cv.dimensions,
        cv.Optional(CONF_FORMAT, default="PNG"): cv.enum(IMAGE_FORMAT, upper=True),
        cv.Optional(CONF_TYPE, default="BINARY"): cv.enum(IMAGE_TYPE, upper=True),
        # Not setting default here on purpose; the default depends on the image type,
        # and thus will be set in the "validate_cross_dependencies" validator.
        cv.Optional(CONF_USE_TRANSPARENCY): cv.boolean,
        cv.Optional(CONF_BUFFER_SIZE, default=2048): cv.int_range(256, 65536),
    }
).extend(cv.polling_component_schema("never"))

CONFIG_SCHEMA = cv.Schema(
    cv.All(
        ONLINE_IMAGE_SCHEMA,
        validate_cross_dependencies,
    )
)


async def to_code(config):
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

    url = config[CONF_URL]
    width, height = config.get(CONF_RESIZE, (0, 0))
    transparent = config[CONF_USE_TRANSPARENCY]

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
    cg.add(var.set_transparency(transparent))
