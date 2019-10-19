import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_HUMIDITY, CONF_ID, CONF_TEMPERATURE, ICON_WATER_PERCENT, \
    ICON_THERMOMETER, UNIT_CELSIUS, UNIT_PERCENT

DEPENDENCIES = ['i2c']

shtcx_ns = cg.esphome_ns.namespace('shtcx')
SHTCXComponent = shtcx_ns.class_('SHTCXComponent', cg.PollingComponent, i2c.I2CDevice)

SHTCXType = shtcx_ns.enum('SHTCXType')

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SHTCXComponent),
    cv.Required(CONF_TEMPERATURE): sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Required(CONF_HUMIDITY): sensor.sensor_schema(UNIT_PERCENT, ICON_WATER_PERCENT, 1),
}).extend(cv.polling_component_schema('60s')).extend(i2c.i2c_device_schema(0x70))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    if CONF_TEMPERATURE in config:
        sens = yield sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_HUMIDITY in config:
        sens = yield sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(sens))
