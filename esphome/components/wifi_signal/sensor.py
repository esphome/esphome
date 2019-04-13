import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_NAME, CONF_UPDATE_INTERVAL, CONF_ICON, \
    CONF_UNIT_OF_MEASUREMENT, CONF_ACCURACY_DECIMALS, ICON_WIFI, UNIT_DECIBEL

DEPENDENCIES = ['wifi']
wifi_signal_ns = cg.esphome_ns.namespace('wifi_signal')
WiFiSignalSensor = wifi_signal_ns.class_('WiFiSignalSensor', sensor.PollingSensorComponent)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(WiFiSignalSensor),
    cv.Optional(CONF_UPDATE_INTERVAL, default='60s'): cv.update_interval,

    cv.Optional(CONF_ICON, default=ICON_WIFI): sensor.icon,
    cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_DECIBEL): sensor.unit_of_measurement,
    cv.Optional(CONF_ACCURACY_DECIMALS, default=0): sensor.accuracy_decimals,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], config[CONF_UPDATE_INTERVAL])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)
