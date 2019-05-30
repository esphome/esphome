import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_ID, CONF_PRESSURE, CONF_TEMPERATURE, \
    UNIT_CELSIUS, ICON_THERMOMETER, ICON_GAUGE, UNIT_HECTOPASCAL

DEPENDENCIES = ['i2c']

bmp085_ns = cg.esphome_ns.namespace('bmp085')
BMP085Component = bmp085_ns.class_('BMP085Component', cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BMP085Component),
    cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Optional(CONF_PRESSURE): sensor.sensor_schema(UNIT_HECTOPASCAL, ICON_GAUGE, 1),
}).extend(cv.polling_component_schema('60s')).extend(i2c.i2c_device_schema(0x77))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    if CONF_TEMPERATURE in config:
        conf = config[CONF_TEMPERATURE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_temperature(sens))

    if CONF_PRESSURE in config:
        conf = config[CONF_PRESSURE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_pressure(sens))
