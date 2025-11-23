import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, ENTITY_CATEGORY_DIAGNOSTIC, ENTITY_CATEGORY_NONE

from . import EzoPMP

DEPENDENCIES = ["ezo_pmp"]

CONF_DOSING_MODE = "dosing_mode"
CONF_CALIBRATION_STATUS = "calibration_status"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(EzoPMP),
        cv.Optional(CONF_DOSING_MODE): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_NONE,
        ),
        cv.Optional(CONF_CALIBRATION_STATUS): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ID])

    if CONF_DOSING_MODE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_DOSING_MODE])
        cg.add(parent.set_dosing_mode(sens))

    if CONF_CALIBRATION_STATUS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_CALIBRATION_STATUS])
        cg.add(parent.set_calibration_status(sens))
