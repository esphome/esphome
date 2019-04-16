from esphome.components import sensor, time
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_NAME, CONF_TIME_ID

DEPENDENCIES = ['time']

CONF_POWER_ID = 'power_id'
total_daily_energy_ns = cg.esphome_ns.namespace('total_daily_energy')
TotalDailyEnergy = total_daily_energy_ns.class_('TotalDailyEnergy', sensor.Sensor, cg.Component)

CONFIG_SCHEMA = cv.nameable(sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TotalDailyEnergy),
    cv.GenerateID(CONF_TIME_ID): cv.use_variable_id(time.RealTimeClock),
    cv.Required(CONF_POWER_ID): cv.use_variable_id(sensor.Sensor),
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    time_ = yield cg.get_variable(config[CONF_TIME_ID])
    sens = yield cg.get_variable(config[CONF_POWER_ID])
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], time_, sens)

    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)
