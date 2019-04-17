import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_HUMIDITY, CONF_ID, CONF_TEMPERATURE, CONF_UPDATE_INTERVAL, \
    ICON_WATER_PERCENT, ICON_THERMOMETER, UNIT_CELSIUS, \
    UNIT_PERCENT

DEPENDENCIES = ['i2c']

sht3xd_ns = cg.esphome_ns.namespace('sht3xd')
SHT3XDComponent = sht3xd_ns.class_('SHT3XDComponent', cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(SHT3XDComponent),
    cv.Required(CONF_TEMPERATURE):
        cv.nameable(sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1)),
    cv.Required(CONF_HUMIDITY):
        cv.nameable(sensor.sensor_schema(UNIT_PERCENT, ICON_WATER_PERCENT, 1)),
    cv.Optional(CONF_UPDATE_INTERVAL, default='60s'): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x44))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_UPDATE_INTERVAL])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    if CONF_TEMPERATURE in config:
        sens = yield sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_HUMIDITY in config:
        sens = yield sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(sens))
