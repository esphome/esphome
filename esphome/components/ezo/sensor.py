import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_ID

CODEOWNERS = ['@ssieb']

DEPENDENCIES = ['i2c']

ezo_ns = cg.esphome_ns.namespace('ezo')

EZOSensor = ezo_ns.class_('EZOSensor', sensor.Sensor, cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(EZOSensor),
}).extend(cv.polling_component_schema('60s')).extend(i2c.i2c_device_schema(None))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)
    yield i2c.register_i2c_device(var, config)
