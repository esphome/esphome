import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DISTANCE,
    CONF_LIGHTNING_ENERGY,
    ICON_FLASH,
    ICON_SIGNAL_DISTANCE_VARIANT,
    UNIT_KILOMETER,
)

from . import AS3935, CONF_AS3935_ID

DEPENDENCIES = ["as3935"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AS3935_ID): cv.use_id(AS3935),
        cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOMETER,
            icon=ICON_SIGNAL_DISTANCE_VARIANT,
            accuracy_decimals=1,
        ),
        cv.Optional(CONF_LIGHTNING_ENERGY): sensor.sensor_schema(
            icon=ICON_FLASH,
            accuracy_decimals=1,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_AS3935_ID])

    if distance_config := config.get(CONF_DISTANCE):
        sens = await sensor.new_sensor(distance_config)
        cg.add(hub.set_distance_sensor(sens))

    if lightning_energy_config := config.get(CONF_LIGHTNING_ENERGY):
        sens = await sensor.new_sensor(lightning_energy_config)
        cg.add(hub.set_energy_sensor(sens))
