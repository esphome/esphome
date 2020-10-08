import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_ID, UNIT_METER, ICON_ARROW_EXPAND_VERTICAL

DEPENDENCIES = ['i2c']

vl53l0x_ns = cg.esphome_ns.namespace('vl53l0x')
VL53L0XSensor = vl53l0x_ns.class_('VL53L0XSensor', sensor.Sensor, cg.PollingComponent,
                                  i2c.I2CDevice)

CONF_SIGNAL_RATE_LIMIT = 'signal_rate_limit'
CONF_LONG_RANGE = 'long_range'

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_METER, ICON_ARROW_EXPAND_VERTICAL, 2).extend({
    cv.GenerateID(): cv.declare_id(VL53L0XSensor),
    cv.Optional(CONF_SIGNAL_RATE_LIMIT, default=0.25): cv.float_range(
        min=0.0, max=512.0, min_included=False, max_included=False),
    cv.Optional(CONF_LONG_RANGE, default=False): cv.boolean,
}).extend(cv.polling_component_schema('60s')).extend(i2c.i2c_device_schema(0x29))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    cg.add(var.set_signal_rate_limit(config[CONF_SIGNAL_RATE_LIMIT]))
    cg.add(var.set_long_range(config[CONF_LONG_RANGE]))
    yield sensor.register_sensor(var, config)
    yield i2c.register_i2c_device(var, config)
