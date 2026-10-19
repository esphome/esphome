import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_CURRENT,
    UNIT_AMPERE,
)
from esphome.types import ConfigType

from . import CONF_DELTA_WALLBOX_ID, DeltaWallbox

DEPENDENCIES = ["delta_wallbox"]

CONF_CHARGING_TIME = "charging_time"
CONF_CHARGER_STATE = "charger_state"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DELTA_WALLBOX_ID): cv.use_id(DeltaWallbox),
        cv.Optional(CONF_CHARGER_STATE): sensor.sensor_schema(
            accuracy_decimals=0,
        ),
        cv.Optional(CONF_MAX_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=1,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_DELTA_WALLBOX_ID])

    if charger_state_config := config.get(CONF_CHARGER_STATE):
        sens = await sensor.new_sensor(charger_state_config)
        cg.add(parent.set_charger_state_sensor(sens))

    if max_current_config := config.get(CONF_MAX_CURRENT):
        sens = await sensor.new_sensor(max_current_config)
        cg.add(parent.set_max_current_sensor(sens))
