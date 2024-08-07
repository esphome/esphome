import logging

from esphome import automation
import esphome.codegen as cg
from esphome.components.http_request import CONF_HTTP_REQUEST_ID, HttpRequestComponent
from esphome.components.image import (
    CONF_USE_TRANSPARENCY,
    IMAGE_TYPE,
    Image_,
    validate_cross_dependencies,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_FORMAT,
    CONF_ID,
    CONF_ON_ERROR,
    CONF_RESIZE,
    CONF_TRIGGER_ID,
    CONF_TYPE,
    CONF_URL,
)

AUTO_LOAD = ["image"]
DEPENDENCIES = ["display", "http_request"]
CODEOWNERS = ["@guillempages"]
MULTI_CONF = True

CONF_ON_DOWNLOAD_FINISHED = "on_download_finished"
CONF_PLACEHOLDER = "placeholder"

_LOGGER = logging.getLogger(__name__)

online_image_ns = cg.esphome_ns.namespace("online_image")

ImageFormat = online_image_ns.enum("ImageFormat")

FORMAT_PNG = "PNG"

IMAGE_FORMAT = {FORMAT_PNG: ImageFormat.PNG}  # Add new supported formats here

OnlineImage = online_image_ns.class_("OnlineImage", cg.PollingComponent, Image_)

# Actions
SetUrlAction = online_image_ns.class_(
    "OnlineImageSetUrlAction", automation.Action, cg.Parented.template(OnlineImage)
)
ReleaseImageAction = online_image_ns.class_(
    "OnlineImageReleaseAction", automation.Action, cg.Parented.template(OnlineImage)
)

# Triggers
DownloadFinishedTrigger = online_image_ns.class_(
    "DownloadFinishedTrigger", automation.Trigger.template()
)
DownloadErrorTrigger = online_image_ns.class_(
    "DownloadErrorTrigger", automation.Trigger.template()
)

ONLINE_IMAGE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(OnlineImage),
        cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
        #
        # Common image options
        #
        cv.Optional(CONF_RESIZE): cv.dimensions,
        cv.Optional(CONF_TYPE, default="BINARY"): cv.enum(IMAGE_TYPE, upper=True),
        # Not setting default here on purpose; the default depends on the image type,
        # and thus will be set in the "validate_cross_dependencies" validator.
        cv.Optional(CONF_USE_TRANSPARENCY): cv.boolean,
        #
        # Online Image specific options
        #
        cv.Required(CONF_URL): cv.url,
        cv.Required(CONF_FORMAT): cv.enum(IMAGE_FORMAT, upper=True),
        cv.Optional(CONF_PLACEHOLDER): cv.use_id(Image_),
        cv.Optional(CONF_BUFFER_SIZE, default=2048): cv.int_range(256, 65536),
        cv.Optional(CONF_ON_DOWNLOAD_FINISHED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DownloadFinishedTrigger),
            }
        ),
        cv.Optional(CONF_ON_ERROR): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DownloadErrorTrigger),
            }
        ),
    }
).extend(cv.polling_component_schema("never"))

CONFIG_SCHEMA = cv.Schema(
    cv.All(
        ONLINE_IMAGE_SCHEMA,
        validate_cross_dependencies,
        cv.require_framework_version(
            # esp8266 not supported yet; if enabled in the future, minimum version of 2.7.0 is needed
            # esp8266_arduino=cv.Version(2, 7, 0),
            esp32_arduino=cv.Version(0, 0, 0),
            esp_idf=cv.Version(4, 0, 0),
        ),
    )
)

SET_URL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(OnlineImage),
        cv.Required(CONF_URL): cv.templatable(cv.url),
    }
)

RELEASE_IMAGE_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(OnlineImage),
    }
)


@automation.register_action("online_image.set_url", SetUrlAction, SET_URL_SCHEMA)
@automation.register_action(
    "online_image.release", ReleaseImageAction, RELEASE_IMAGE_SCHEMA
)
async def online_image_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    if CONF_URL in config:
        template_ = await cg.templatable(config[CONF_URL], args, cg.const_char_ptr)
        cg.add(var.set_url(template_))
    return var


async def to_code(config):
    format = config[CONF_FORMAT]
    if format in [FORMAT_PNG]:
        cg.add_define("USE_ONLINE_IMAGE_PNG_SUPPORT")
        cg.add_library("pngle", "1.0.2")

    url = config[CONF_URL]
    width, height = config.get(CONF_RESIZE, (0, 0))
    transparent = config[CONF_USE_TRANSPARENCY]

    var = cg.new_Pvariable(
        config[CONF_ID],
        url,
        width,
        height,
        format,
        config[CONF_TYPE],
        config[CONF_BUFFER_SIZE],
    )
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_HTTP_REQUEST_ID])

    cg.add(var.set_transparency(transparent))

    if placeholder_id := config.get(CONF_PLACEHOLDER):
        placeholder = await cg.get_variable(placeholder_id)
        cg.add(var.set_placeholder(placeholder))

    for conf in config.get(CONF_ON_DOWNLOAD_FINISHED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_ERROR, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
