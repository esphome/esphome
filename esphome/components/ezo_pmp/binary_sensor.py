import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    ENTITY_CATEGORY_NONE,
    DEVICE_CLASS_RUNNING,
    DEVICE_CLASS_EMPTY,
    CONF_ID,
)

from . import EzoPMP

DEPENDENCIES = ["ezo_pmp"]

CONF_PUMP_STATE = "pump_state"
CONF_IS_PAUSED = "is_paused"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(EzoPMP),
        cv.Optional(CONF_PUMP_STATE): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_RUNNING,
            entity_category=ENTITY_CATEGORY_NONE,
        ),
        cv.Optional(CONF_IS_PAUSED): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_EMPTY,
            entity_category=ENTITY_CATEGORY_NONE,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ID])

    if CONF_PUMP_STATE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_PUMP_STATE])
        cg.add(parent.set_is_dosing(sens))

    if CONF_IS_PAUSED in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_IS_PAUSED])
        cg.add(parent.set_is_paused(sens))
