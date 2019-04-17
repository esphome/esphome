import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_ID, CONF_PRESSURE, \
    CONF_TEMPERATURE, CONF_UPDATE_INTERVAL, ICON_THERMOMETER, UNIT_CELSIUS, ICON_GAUGE, \
    UNIT_HECTOPASCAL

DEPENDENCIES = ['i2c']

ms5611_ns = cg.esphome_ns.namespace('ms5611')
MS5611Component = ms5611_ns.class_('MS5611Component', cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(MS5611Component),
    cv.Required(CONF_TEMPERATURE): cv.nameable(
        sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1)),
    cv.Required(CONF_PRESSURE): cv.nameable(sensor.sensor_schema(UNIT_HECTOPASCAL, ICON_GAUGE, 1)),
    cv.Optional(CONF_UPDATE_INTERVAL, default='60s'): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x77))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_UPDATE_INTERVAL])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    if CONF_TEMPERATURE in config:
        sens = yield sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_PRESSURE in config:
        sens = yield sensor.new_sensor(config[CONF_PRESSURE])
        cg.add(var.set_pressure_sensor(sens))
