import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core.entity_helpers import setup_entity
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/tests"]
AUTO_LOAD = ["camera"]

CONF_IMAGE_SIZE = "image_size"
CONF_FRAME_INTERVAL = "frame_interval"

mock_camera_ns = cg.esphome_ns.namespace("mock_camera")
MockCamera = mock_camera_ns.class_("MockCamera", cg.Component, cg.EntityBase)

CONFIG_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(MockCamera),
        cv.Optional(CONF_IMAGE_SIZE, default=1024): cv.positive_not_null_int,
        cv.Optional(
            CONF_FRAME_INTERVAL, default="50ms"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_CAMERA")
    var = cg.new_Pvariable(config[CONF_ID])
    await setup_entity(var, config, "camera")
    await cg.register_component(var, config)
    cg.add(var.set_image_size(config[CONF_IMAGE_SIZE]))
    cg.add(var.set_frame_interval(config[CONF_FRAME_INTERVAL]))
