import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_PROBLEM, ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_UFM01_ID, UFM01Component

DEPENDENCIES = ["ufm01"]

CONF_UFC_CHIP_ERROR = "ufc_chip_error"
CONF_FLOW_DIRECTION_WRONG = "flow_direction_wrong"
CONF_EMPTY_TUBE = "empty_tube"
CONF_FLOW_RATE_OUT_OF_RANGE = "flow_rate_out_of_range"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_UFM01_ID): cv.use_id(UFM01Component),
    cv.Optional(CONF_UFC_CHIP_ERROR): binary_sensor.binary_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, device_class=DEVICE_CLASS_PROBLEM
    ),
    cv.Optional(CONF_FLOW_DIRECTION_WRONG): binary_sensor.binary_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        device_class=DEVICE_CLASS_PROBLEM,
    ),
    cv.Optional(CONF_EMPTY_TUBE): binary_sensor.binary_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        device_class=DEVICE_CLASS_PROBLEM,
    ),
    cv.Optional(CONF_FLOW_RATE_OUT_OF_RANGE): binary_sensor.binary_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        device_class=DEVICE_CLASS_PROBLEM,
    ),
}


async def to_code(config):
    ufm01_component = await cg.get_variable(config[CONF_UFM01_ID])

    if ufc_chip_error_config := config.get(CONF_UFC_CHIP_ERROR):
        sens = await binary_sensor.new_binary_sensor(ufc_chip_error_config)
        cg.add(ufm01_component.set_ufc_chip_error_binary_sensor(sens))

    if flow_direction_wrong_config := config.get(CONF_FLOW_DIRECTION_WRONG):
        sens = await binary_sensor.new_binary_sensor(flow_direction_wrong_config)
        cg.add(ufm01_component.set_flow_direction_wrong_binary_sensor(sens))

    if empty_tube_config := config.get(CONF_EMPTY_TUBE):
        sens = await binary_sensor.new_binary_sensor(empty_tube_config)
        cg.add(ufm01_component.set_empty_tube_binary_sensor(sens))

    if flow_rate_out_of_range_config := config.get(CONF_FLOW_RATE_OUT_OF_RANGE):
        sens = await binary_sensor.new_binary_sensor(flow_rate_out_of_range_config)
        cg.add(ufm01_component.set_flow_rate_out_of_range_binary_sensor(sens))
