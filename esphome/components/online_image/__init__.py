import logging

from esphome import automation
import esphome.codegen as cg
from esphome.components.http_request import CONF_HTTP_REQUEST_ID, HttpRequestComponent
from esphome.components.image import (
    CONF_INVERT_ALPHA,
    CONF_TRANSPARENCY,
    IMAGE_SCHEMA,
    Image_,
    get_image_type_enum,
    get_transparency_enum,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_DITHER,
    CONF_FILE,
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
CODEOWNERS = ["@guillempages", "@clydebarrow"]
MULTI_CONF = True

CONF_ON_DOWNLOAD_FINISHED = "on_download_finished"
CONF_PLACEHOLDER = "placeholder"

_LOGGER = logging.getLogger(__name__)

online_image_ns = cg.esphome_ns.namespace("online_image")

ImageFormat = online_image_ns.enum("ImageFormat")


class Format:
    def __init__(self, image_type):
        self.image_type = image_type

    @property
    def enum(self):
        return getattr(ImageFormat, self.image_type)

    def actions(self):
        pass


class BMPFormat(Format):
    def __init__(self):
        super().__init__("BMP")

    def actions(self):
        cg.add_define("USE_ONLINE_IMAGE_BMP_SUPPORT")


class JPEGFormat(Format):
    def __init__(self):
        super().__init__("JPEG")

    def actions(self):
        cg.add_define("USE_ONLINE_IMAGE_JPEG_SUPPORT")
        cg.add_library("JPEGDEC", None, "https://github.com/bitbank2/JPEGDEC#ca1e0f2")


class PNGFormat(Format):
    def __init__(self):
        super().__init__("PNG")

    def actions(self):
        cg.add_define("USE_ONLINE_IMAGE_PNG_SUPPORT")
        cg.add_library("pngle", "1.1.0")


IMAGE_FORMATS = {
    x.image_type: x
    for x in (
        BMPFormat(),
        JPEGFormat(),
        PNGFormat(),
    )
}
IMAGE_FORMATS.update({"JPG": IMAGE_FORMATS["JPEG"]})

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


def remove_options(*options):
    return {
        cv.Optional(option): cv.invalid(
            f"{option} is an invalid option for online_image"
        )
        for option in options
    }


ONLINE_IMAGE_SCHEMA = (
    IMAGE_SCHEMA.extend(remove_options(CONF_FILE, CONF_INVERT_ALPHA, CONF_DITHER))
    .extend(
        {
            cv.Required(CONF_ID): cv.declare_id(OnlineImage),
            cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
            # Online Image specific options
            cv.Required(CONF_URL): cv.url,
            cv.Required(CONF_FORMAT): cv.one_of(*IMAGE_FORMATS, upper=True),
            cv.Optional(CONF_PLACEHOLDER): cv.use_id(Image_),
            cv.Optional(CONF_BUFFER_SIZE, default=65536): cv.int_range(256, 65536),
            cv.Optional(CONF_ON_DOWNLOAD_FINISHED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        DownloadFinishedTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_ERROR): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DownloadErrorTrigger),
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("never"))
)

CONFIG_SCHEMA = cv.Schema(
    cv.All(
        ONLINE_IMAGE_SCHEMA,
        cv.require_framework_version(
            # esp8266 not supported yet; if enabled in the future, minimum version of 2.7.0 is needed
            # esp8266_arduino=cv.Version(2, 7, 0),
            esp32_arduino=cv.Version(0, 0, 0),
            esp_idf=cv.Version(4, 0, 0),
            rp2040_arduino=cv.Version(0, 0, 0),
            host=cv.Version(0, 0, 0),
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
        template_ = await cg.templatable(config[CONF_URL], args, cg.std_string)
        cg.add(var.set_url(template_))
    return var


async def to_code(config):
    image_format = IMAGE_FORMATS[config[CONF_FORMAT]]
    image_format.actions()

    url = config[CONF_URL]
    width, height = config.get(CONF_RESIZE, (0, 0))
    transparent = get_transparency_enum(config[CONF_TRANSPARENCY])

    var = cg.new_Pvariable(
        config[CONF_ID],
        url,
        width,
        height,
        image_format.enum,
        get_image_type_enum(config[CONF_TYPE]),
        transparent,
        config[CONF_BUFFER_SIZE],
    )
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_HTTP_REQUEST_ID])

    if placeholder_id := config.get(CONF_PLACEHOLDER):
        placeholder = await cg.get_variable(placeholder_id)
        cg.add(var.set_placeholder(placeholder))

    for conf in config.get(CONF_ON_DOWNLOAD_FINISHED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_ERROR, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
