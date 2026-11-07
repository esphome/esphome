import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_RUNNING,
    ENTITY_CATEGORY_NONE,
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
    hub = await cg.get_variable(config[CONF_ID])

    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_PUMP_STATE, hub.set_is_dosing)
    await binary_sensors(CONF_IS_PAUSED, hub.set_is_paused)
