import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from . import AS3935, CONF_AS3935_ID

DEPENDENCIES = ['as3935']

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(CONF_AS3935_ID): cv.use_id(AS3935),
})


def to_code(config):
    hub = yield cg.get_variable(config[CONF_AS3935_ID])
    var = yield binary_sensor.new_binary_sensor(config)
    cg.add(hub.set_thunder_alert_binary_sensor(var))
