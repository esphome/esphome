import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ENERGY,
    CONF_MAX_CURRENT,
    CONF_POWER,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_KILOWATT,
    UNIT_KILOWATT_HOURS,
    UNIT_MINUTE,
)
from esphome.types import ConfigType

from . import CONF_DELTA_WALLBOX_ID, DeltaWallbox

DEPENDENCIES = ["delta_wallbox"]

CONF_CHARGING_TIME = "charging_time"
CONF_CHARGER_STATE = "charger_state"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DELTA_WALLBOX_ID): cv.use_id(DeltaWallbox),
        cv.Optional(CONF_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=3,
        ),
        cv.Optional(CONF_ENERGY): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            accuracy_decimals=3,
        ),
        cv.Optional(CONF_CHARGING_TIME): sensor.sensor_schema(
            unit_of_measurement=UNIT_MINUTE,
            accuracy_decimals=0,
        ),
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

    if power_config := config.get(CONF_POWER):
        sens = await sensor.new_sensor(power_config)
        cg.add(parent.set_power_sensor(sens))

    if energy_config := config.get(CONF_ENERGY):
        sens = await sensor.new_sensor(energy_config)
        cg.add(parent.set_energy_sensor(sens))

    if charging_time_config := config.get(CONF_CHARGING_TIME):
        sens = await sensor.new_sensor(charging_time_config)
        cg.add(parent.set_charging_time_sensor(sens))

    if charger_state_config := config.get(CONF_CHARGER_STATE):
        sens = await sensor.new_sensor(charger_state_config)
        cg.add(parent.set_charger_state_sensor(sens))

    if max_current_config := config.get(CONF_MAX_CURRENT):
        sens = await sensor.new_sensor(max_current_config)
        cg.add(parent.set_max_current_sensor(sens))
