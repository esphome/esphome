import logging

from esphome import automation
import esphome.codegen as cg
from esphome.components import runtime_image
from esphome.components.const import CONF_REQUEST_HEADERS
from esphome.components.http_request import CONF_HTTP_REQUEST_ID, HttpRequestComponent
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_ID,
    CONF_ON_ERROR,
    CONF_TRIGGER_ID,
    CONF_URL,
)
from esphome.core import Lambda

AUTO_LOAD = ["image", "runtime_image"]
DEPENDENCIES = ["display", "http_request"]
CODEOWNERS = ["@guillempages", "@clydebarrow"]
MULTI_CONF = True

CONF_ON_DOWNLOAD_FINISHED = "on_download_finished"
CONF_UPDATE = "update"

_LOGGER = logging.getLogger(__name__)

online_image_ns = cg.esphome_ns.namespace("online_image")

OnlineImage = online_image_ns.class_(
    "OnlineImage", cg.PollingComponent, runtime_image.RuntimeImage
)

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


ONLINE_IMAGE_SCHEMA = (
    runtime_image.runtime_image_schema(OnlineImage)
    .extend(
        {
            # Online Image specific options
            cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
            cv.Required(CONF_URL): cv.url,
            cv.Optional(CONF_BUFFER_SIZE, default=65536): cv.int_range(256, 65536),
            cv.Optional(CONF_REQUEST_HEADERS): cv.All(
                cv.Schema({cv.string: cv.templatable(cv.string)})
            ),
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
        runtime_image.validate_runtime_image_settings,
    )
)

SET_URL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(OnlineImage),
        cv.Required(CONF_URL): cv.templatable(cv.url),
        cv.Optional(CONF_UPDATE, default=True): cv.templatable(bool),
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
    if CONF_UPDATE in config:
        template_ = await cg.templatable(config[CONF_UPDATE], args, bool)
        cg.add(var.set_update(template_))
    return var


async def to_code(config):
    # Use the enhanced helper function to get all runtime image parameters
    settings = await runtime_image.process_runtime_image_config(config)

    url = config[CONF_URL]
    var = cg.new_Pvariable(
        config[CONF_ID],
        url,
        settings.width,
        settings.height,
        settings.format_enum,
        settings.image_type_enum,
        settings.transparent,
        settings.placeholder or cg.nullptr,
        config[CONF_BUFFER_SIZE],
        settings.byte_order_big_endian,
    )
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_HTTP_REQUEST_ID])

    for key, value in config.get(CONF_REQUEST_HEADERS, {}).items():
        if isinstance(value, Lambda):
            template_ = await cg.templatable(value, [], cg.std_string)
            cg.add(var.add_request_header(key, template_))
        else:
            cg.add(var.add_request_header(key, value))

    for conf in config.get(CONF_ON_DOWNLOAD_FINISHED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(bool, "cached")], conf)

    for conf in config.get(CONF_ON_ERROR, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
