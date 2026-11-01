import esphome.codegen as cg
from esphome.components import runtime_image
from esphome.components.image import CONF_TRANSPARENCY, add_metadata
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE
from esphome.types import ConfigType

AUTO_LOAD = ["runtime_image"]
CODEOWNERS = ["@FuNK3Y"]

# An image whose pixels are decoded at runtime from a buffer the configuration
# already holds, rather than from a file at build time or a URL. The decoder is
# driven from a lambda, as online_image drives it from its download:
#   id(img).begin_decode(len); id(img).feed_data(buf, len); id(img).end_decode();
CONFIG_SCHEMA = cv.All(
    runtime_image.runtime_image_schema(runtime_image.RuntimeImage),
    runtime_image.validate_runtime_image_settings,
)


async def to_code(config: ConfigType) -> None:
    settings = await runtime_image.process_runtime_image_config(config)
    # Consumers such as LVGL resolve an image's dimensions and format at codegen
    # time, so they are declared here even though the pixels arrive later.
    add_metadata(
        config[CONF_ID],
        settings.width,
        settings.height,
        config[CONF_TYPE],
        config[CONF_TRANSPARENCY],
    )
    cg.new_Pvariable(
        config[CONF_ID],
        settings.format_enum,
        settings.image_type_enum,
        settings.transparent,
        settings.placeholder or cg.nullptr,
        settings.byte_order_big_endian,
        settings.width,
        settings.height,
    )
