import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_HUMIDITY, CONF_ID, CONF_TEMPERATURE, \
    ICON_THERMOMETER, ICON_WATER_PERCENT, UNIT_CELSIUS, UNIT_PERCENT

DEPENDENCIES = ['i2c']

hdc1080_ns = cg.esphome_ns.namespace('hdc1080')
HDC1080Component = hdc1080_ns.class_('HDC1080Component', cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(HDC1080Component),
    cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(UNIT_PERCENT, ICON_WATER_PERCENT, 0),
}).extend(cv.polling_component_schema('60s')).extend(i2c.i2c_device_schema(0x40))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    if CONF_TEMPERATURE in config:
        sens = yield sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature(sens))

    if CONF_HUMIDITY in config:
        sens = yield sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity(sens))
