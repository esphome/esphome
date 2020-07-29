import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, ICON_TIMER, UNIT_SECOND

uptime_ns = cg.esphome_ns.namespace('uptime')
UptimeSensor = uptime_ns.class_('UptimeSensor', sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_SECOND, ICON_TIMER, 0).extend({
    cv.GenerateID(): cv.declare_id(UptimeSensor),
}).extend(cv.polling_component_schema('60s'))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)
