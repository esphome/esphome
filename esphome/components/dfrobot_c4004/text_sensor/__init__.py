import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from .. import CONF_C4004_ID, C4004Component

CONF_DETECTION_RANGE_MODE = "detection_range_mode"
CONF_C4004_STATUS = "c4004_status"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_C4004_ID): cv.use_id(C4004Component),
        cv.Optional(CONF_DETECTION_RANGE_MODE): text_sensor.text_sensor_schema(
            icon="mdi:map-marker-radius",
        ),
        cv.Optional(CONF_C4004_STATUS): text_sensor.text_sensor_schema(
            icon="mdi:message-text-outline",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_C4004_ID])

    if mode_config := config.get(CONF_DETECTION_RANGE_MODE):
        sens = await text_sensor.new_text_sensor(mode_config)
        cg.add(parent.set_detection_range_mode_text_sensor(sens))

    if status_config := config.get(CONF_C4004_STATUS):
        sens = await text_sensor.new_text_sensor(status_config)
        cg.add(parent.set_status_text_sensor(sens))
