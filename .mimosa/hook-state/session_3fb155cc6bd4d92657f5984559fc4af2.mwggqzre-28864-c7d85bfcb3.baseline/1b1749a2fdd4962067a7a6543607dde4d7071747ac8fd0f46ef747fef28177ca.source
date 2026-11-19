import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
)

from . import CONF_SIM800L_ID, Sim800LComponent

DEPENDENCIES = ["sim800l"]

CONF_RSSI = "rssi"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_SIM800L_ID): cv.use_id(Sim800LComponent),
    cv.Optional(CONF_RSSI): sensor.sensor_schema(
        unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}


async def to_code(config):
    hub = await cg.get_variable(config[CONF_SIM800L_ID])

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_RSSI, hub.set_rssi_sensor)
