import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DISTANCE, CONF_LIGHTNING_ENERGY, ICON_FLASH, ICON_SIGNAL_DISTANCE_VARIANT, UNIT_EMPTY,
    UNIT_KILOMETER,
)

from . import AS3935, CONF_AS3935_ID

DEPENDENCIES = ['as3935']

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_AS3935_ID): cv.use_id(AS3935),
    cv.Optional(CONF_DISTANCE):
        sensor.sensor_schema(UNIT_KILOMETER, ICON_SIGNAL_DISTANCE_VARIANT, 1),
    cv.Optional(CONF_LIGHTNING_ENERGY):
        sensor.sensor_schema(UNIT_EMPTY, ICON_FLASH, 1),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    hub = yield cg.get_variable(config[CONF_AS3935_ID])

    if CONF_DISTANCE in config:
        conf = config[CONF_DISTANCE]
        distance_sensor = yield sensor.new_sensor(conf)
        cg.add(hub.set_distance_sensor(distance_sensor))

    if CONF_LIGHTNING_ENERGY in config:
        conf = config[CONF_LIGHTNING_ENERGY]
        lightning_energy_sensor = yield sensor.new_sensor(conf)
        cg.add(hub.set_energy_sensor(lightning_energy_sensor))
