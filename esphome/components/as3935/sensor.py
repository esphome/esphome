import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DISTANCE,
    CONF_LIGHTNING_ENERGY,
    ICON_FLASH,
    ICON_SIGNAL_DISTANCE_VARIANT,
    STATE_CLASS_MEASUREMENT,
    UNIT_KILOMETER,
)
from esphome.types import ConfigType

from . import AS3935, CONF_AS3935_ID

DEPENDENCIES = ["as3935"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AS3935_ID): cv.use_id(AS3935),
        cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOMETER,
            icon=ICON_SIGNAL_DISTANCE_VARIANT,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_LIGHTNING_ENERGY): sensor.sensor_schema(
            icon=ICON_FLASH,
            accuracy_decimals=1,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_AS3935_ID])

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_DISTANCE, hub.set_distance_sensor)
    await sensors(CONF_LIGHTNING_ENERGY, hub.set_energy_sensor)
