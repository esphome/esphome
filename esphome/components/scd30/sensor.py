import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_ID, UNIT_PARTS_PER_MILLION, \
    CONF_HUMIDITY, CONF_TEMPERATURE, ICON_PERIODIC_TABLE_CO2, \
    UNIT_CELSIUS, ICON_THERMOMETER, ICON_WATER_PERCENT, UNIT_PERCENT, CONF_CO2

DEPENDENCIES = ['i2c']

scd30_ns = cg.esphome_ns.namespace('scd30')
SCD30Component = scd30_ns.class_('SCD30Component', cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SCD30Component),
    cv.Required(CONF_CO2): sensor.sensor_schema(UNIT_PARTS_PER_MILLION,
                                                ICON_PERIODIC_TABLE_CO2, 0),
    cv.Required(CONF_TEMPERATURE): sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Required(CONF_HUMIDITY): sensor.sensor_schema(UNIT_PERCENT, ICON_WATER_PERCENT, 1),
}).extend(cv.polling_component_schema('60s')).extend(i2c.i2c_device_schema(0x61))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    if CONF_CO2 in config:
        sens = yield sensor.new_sensor(config[CONF_CO2])
        cg.add(var.set_co2_sensor(sens))

    if CONF_HUMIDITY in config:
        sens = yield sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(sens))

    if CONF_TEMPERATURE in config:
        sens = yield sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
