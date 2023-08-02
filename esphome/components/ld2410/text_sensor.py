import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from . import CONF_LD2410_ID, LD2410Component

DEPENDENCIES = ["ld2410"]
CONF_FW_VERSION = "fw_version"
CONF_INFO_QUERY = "info_query"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
    cv.Optional(CONF_FW_VERSION): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_INFO_QUERY): text_sensor.text_sensor_schema(),
}

async def to_code(config):
    ld2410_component = await cg.get_variable(config[CONF_LD2410_ID])
    if CONF_FW_VERSION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_FW_VERSION])
        cg.add(ld2410_component.set_fw_version_sensor(sens))
    if CONF_INFO_QUERY in config:
        sens_info = await text_sensor.new_text_sensor(config[CONF_INFO_QUERY])
        cg.add(ld2410_component.set_info_query_sensor(sens_info))
