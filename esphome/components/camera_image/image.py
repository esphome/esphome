"""Camera image platform: expose the camera's frames as an ``image``."""

from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32_camera, runtime_image
from esphome.components.image import CONF_ALPHA_CHANNEL, CONF_TRANSPARENCY, add_metadata
from esphome.components.runtime_image import CONF_DOUBLE_BUFFER
import esphome.config_validation as cv
from esphome.const import (
    CONF_HEIGHT,
    CONF_ID,
    CONF_ON_ERROR,
    CONF_RESIZE,
    CONF_ROTATION,
    CONF_TYPE,
    CONF_WIDTH,
    CONF_X,
    CONF_Y,
)
from esphome.types import ConfigType

AUTO_LOAD = ["runtime_image"]
DEPENDENCIES = ["esp32_camera"]
CODEOWNERS = ["@nliaudat"]

CONF_CAMERA_ID = "camera_id"
CONF_CROP = "crop"
CONF_ON_IMAGE = "on_image"

_CROP_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_X): cv.int_range(min=0, max=8192),
        cv.Required(CONF_Y): cv.int_range(min=0, max=8192),
        cv.Required(CONF_WIDTH): cv.int_range(min=1, max=8192),
        cv.Required(CONF_HEIGHT): cv.int_range(min=1, max=8192),
    }
)

camera_image_ns = cg.esphome_ns.namespace("camera_image")
CameraImage = camera_image_ns.class_(
    "CameraImage", cg.PollingComponent, runtime_image.RuntimeImage
)


def _validate_options(config: ConfigType) -> ConfigType:
    transforms = CONF_CROP in config or config[CONF_ROTATION] != 0
    if transforms and config[CONF_DOUBLE_BUFFER]:
        raise cv.Invalid(
            "'crop' and 'rotation' cannot be combined with 'double_buffer'"
        )
    if transforms and config[CONF_TYPE] not in ("GRAYSCALE", "RGB565", "RGB"):
        raise cv.Invalid("'crop' and 'rotation' require type GRAYSCALE, RGB565 or RGB")
    # With an alpha channel the image keeps a separate plane next to the colour
    # data, and crop/rotation only transforms the colour plane.
    if transforms and config[CONF_TRANSPARENCY] == CONF_ALPHA_CHANNEL:
        raise cv.Invalid(
            "'crop' and 'rotation' cannot be combined with "
            "'transparency: alpha_channel'"
        )
    if (
        config[CONF_ROTATION] != 0
        and CONF_CROP not in config
        and CONF_RESIZE not in config
    ):
        raise cv.Invalid("'rotation' requires 'resize' or 'crop'")
    return config


CONFIG_SCHEMA = cv.All(
    runtime_image.runtime_image_schema(CameraImage)
    .extend(
        {
            cv.Required(CONF_CAMERA_ID): cv.use_id(esp32_camera.ESP32Camera),
            cv.Optional(CONF_DOUBLE_BUFFER, default=False): cv.boolean,
            cv.Optional(CONF_CROP): _CROP_SCHEMA,
            cv.Optional(CONF_ROTATION, default=0): cv.one_of(0, 90, 180, 270),
            cv.Optional(CONF_ON_IMAGE): automation.validate_automation({}),
            cv.Optional(CONF_ON_ERROR): automation.validate_automation({}),
        }
    )
    .extend(cv.polling_component_schema("never")),
    _validate_options,
    cv.only_on_esp32,
)

_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(CONF_ON_IMAGE, "add_on_image_callback"),
    automation.CallbackAutomation(CONF_ON_ERROR, "add_on_error_callback"),
)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_CAMERA_IMAGE")

    settings = await runtime_image.process_runtime_image_config(config)

    metadata_width = settings.width
    metadata_height = settings.height
    if CONF_CROP in config:
        metadata_width = config[CONF_CROP][CONF_WIDTH]
        metadata_height = config[CONF_CROP][CONF_HEIGHT]
    if config[CONF_ROTATION] in (90, 270):
        metadata_width, metadata_height = metadata_height, metadata_width

    add_metadata(
        config[CONF_ID],
        metadata_width,
        metadata_height,
        config[CONF_TYPE],
        config[CONF_TRANSPARENCY],
    )

    camera_var = await cg.get_variable(config[CONF_CAMERA_ID])
    var = cg.new_Pvariable(
        config[CONF_ID],
        camera_var,
        settings.width,
        settings.height,
        settings.format_enum,
        settings.image_type_enum,
        settings.transparent,
        settings.placeholder or cg.nullptr,
        settings.byte_order_big_endian,
    )
    await cg.register_component(var, config)

    if settings.double_buffer:
        cg.add(var.set_double_buffer(True))

    if CONF_CROP in config:
        crop = config[CONF_CROP]
        cg.add(
            var.set_crop(
                crop[CONF_X], crop[CONF_Y], crop[CONF_WIDTH], crop[CONF_HEIGHT]
            )
        )
    if config[CONF_ROTATION]:
        cg.add(var.set_rotation(config[CONF_ROTATION]))
    if CONF_CROP in config or config[CONF_ROTATION] != 0:
        cg.add(var.set_transform_size(metadata_width, metadata_height))

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)
