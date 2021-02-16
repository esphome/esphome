import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_ID, UNIT_METER, ICON_ARROW_EXPAND_VERTICAL
from esphome.core import TimePeriod

DEPENDENCIES = ['i2c']

vl53l1x_ns = cg.esphome_ns.namespace('vl53l1x')
VL53L1XSensor = vl53l1x_ns.class_('VL53L1XSensor', sensor.Sensor, cg.PollingComponent,
                                  i2c.I2CDevice)
vl53l1x_distance_mode = vl53l1x_ns.enum('vl53l1x_distance_mode')
vl53l1x_distance_modes = {
    'SHORT': vl53l1x_distance_mode.SHORT,
    'MEDIUM': vl53l1x_distance_mode.MEDIUM,
    'LONG': vl53l1x_distance_mode.LONG,
}

CONF_DISTANCE_MODE = "distance_mode"
CONF_TIMING_BUDGET = 'timing_budget'
CONF_RETRY_BUDGET = 'retry_budget'

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_METER, ICON_ARROW_EXPAND_VERTICAL, 2).extend({
    cv.GenerateID(): cv.declare_id(VL53L1XSensor),
    cv.Optional(CONF_DISTANCE_MODE, default="LONG"): cv.enum(vl53l1x_distance_modes, upper=True),
    cv.Optional(CONF_TIMING_BUDGET, default='50ms'):
        cv.All(cv.positive_time_period_microseconds,
               cv.Range(min=TimePeriod(microseconds=20000),
                        max=TimePeriod(microseconds=1100000))),
    cv.Optional(CONF_RETRY_BUDGET, default=5): cv.int_range(min=0, max=255),
}).extend(cv.polling_component_schema('60s')).extend(i2c.i2c_device_schema(0x29))

SETTERS = {

  CONF_DISTANCE_MODE = 'set_distance_mode',
  CONF_TIMING_BUDGET = 'set_timing_budget',
  CONF_RETRY_BUDGET = 'set_retry_budget',

}

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    for key, setter in SETTERS.items():
        if key in config:
            cg.add(getattr(var, setter)(config[key]))

    yield sensor.register_sensor(var, config)
    yield i2c.register_i2c_device(var, config)
