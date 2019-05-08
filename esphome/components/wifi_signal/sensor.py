import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, ICON_WIFI, UNIT_DECIBEL

DEPENDENCIES = ['wifi']
wifi_signal_ns = cg.esphome_ns.namespace('wifi_signal')
WiFiSignalSensor = wifi_signal_ns.class_('WiFiSignalSensor', sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_DECIBEL, ICON_WIFI, 0).extend({
    cv.GenerateID(): cv.declare_id(WiFiSignalSensor),
}).extend(cv.polling_component_schema('60s'))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)
