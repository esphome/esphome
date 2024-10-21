import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC
from . import CONF_MR60FDA2_ID, MR60FDA2Component

AUTO_LOAD = ["seeed_mr60fda2"]

CONF_IS_FALL = "is_fall"

# The entity category for read only diagnostic values, for example RSSI, uptime or MAC Address
CONFIG_SCHEMA = {
    cv.GenerateID(CONF_MR60FDA2_ID): cv.use_id(MR60FDA2Component),
    cv.Optional(CONF_IS_FALL): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:walk"
    ),
}


async def to_code(config):
    mr60fda2_component = await cg.get_variable(config[CONF_MR60FDA2_ID])
    if is_fall_config := config.get(CONF_IS_FALL):
        sens = await text_sensor.new_text_sensor(is_fall_config)
        cg.add(mr60fda2_component.set_is_fall_text_sensor(sens))
