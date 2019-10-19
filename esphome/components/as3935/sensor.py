import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_DISTANCE, CONF_LIGHTNING_ENERGY, \
    UNIT_KILOMETER, UNIT_EMPTY, ICON_SIGNAL_DISTANCE_VARIANT, ICON_FLASH
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
        cg.add(hub.set_distance_sensor(lightning_energy_sensor))
