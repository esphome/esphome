import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_ID, CONF_ADDRESS, \
    CONF_TEMPERATURE, UNIT_CELSIUS, ICON_THERMOMETER, \
    CONF_PRESSURE, UNIT_HECTOPASCAL, ICON_GAUGE, \
    CONF_HUMIDITY, UNIT_PERCENT, ICON_WATER_PERCENT \

DEPENDENCIES = ['i2c']

ms8607_ns = cg.esphome_ns.namespace('ms8607')
MS8607Component = ms8607_ns.class_('MS8607Component', cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(MS8607Component),
    # TODO: can/should these be optional to ignore sensors we don't care about?
    cv.Required(CONF_TEMPERATURE): sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Required(CONF_PRESSURE): sensor.sensor_schema(UNIT_HECTOPASCAL, ICON_GAUGE, 1),
    cv.Required(CONF_HUMIDITY): sensor.sensor_schema(UNIT_PERCENT, ICON_WATER_PERCENT, 2)
        # Humidity is at a different I2C address, but same physical device/I2C bus
        .extend(cv.Schema({ cv.Optional(CONF_ADDRESS, default=0x40): cv.i2c_address }))
}).extend(cv.polling_component_schema('60s')).extend(i2c.i2c_device_schema(0x76))

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    if CONF_TEMPERATURE in config:
        sens = yield sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_PRESSURE in config:
        sens = yield sensor.new_sensor(config[CONF_PRESSURE])
        cg.add(var.set_pressure_sensor(sens))

    if CONF_HUMIDITY in config:
        sens = yield sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(sens))
        # C++ class creates the I2CDevice object, using I2C bus already provided to the object
        cg.add(var.set_humidity_sensor_address(config[CONF_HUMIDITY][CONF_ADDRESS]))
