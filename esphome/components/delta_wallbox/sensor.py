import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ENERGY,
    CONF_MAX_CURRENT,
    CONF_POWER,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_POWER,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_KILOWATT,
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
        cv.Optional(CONF_CHARGER_STATE): sensor.sensor_schema(
            accuracy_decimals=0,
        ),
        cv.Optional(CONF_MAX_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=1,
        ),
        cv.Optional(CONF_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ENERGY): sensor.sensor_schema(
            kind=sensor.ENERGY_KWH, accuracy_decimals=2
        ),
        cv.Optional(CONF_CHARGING_TIME): sensor.sensor_schema(
            unit_of_measurement=UNIT_MINUTE,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DELTA_WALLBOX_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_CHARGER_STATE, hub.set_charger_state_sensor)
    await sensors(CONF_MAX_CURRENT, hub.set_max_current_sensor)
    await sensors(CONF_POWER, hub.set_power_sensor)
    await sensors(CONF_ENERGY, hub.set_energy_sensor)
    await sensors(CONF_CHARGING_TIME, hub.set_charging_time_sensor)
