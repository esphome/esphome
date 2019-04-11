from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_NAME, CONF_UPDATE_INTERVAL, CONF_UNIT_OF_MEASUREMENT, \
    UNIT_SECOND, ICON_TIMER, CONF_ICON, CONF_ACCURACY_DECIMALS

uptime_ns = cg.esphome_ns.namespace('uptime')
UptimeSensor = uptime_ns.class_('UptimeSensor', sensor.PollingSensorComponent)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(UptimeSensor),

    cv.Optional(CONF_UPDATE_INTERVAL, default='60s'): cv.update_interval,
    cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_SECOND): sensor.unit_of_measurement,
    cv.Optional(CONF_ICON, default=ICON_TIMER): sensor.icon,
    cv.Optional(CONF_ACCURACY_DECIMALS, default=0): sensor.accuracy_decimals,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    rhs = UptimeSensor.new(config[CONF_NAME], config[CONF_UPDATE_INTERVAL])
    uptime = cg.Pvariable(config[CONF_ID], rhs)
    yield cg.register_component(uptime, config)
    yield sensor.register_sensor(uptime, config)
