import voluptuous as vol

from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_BSSID, CONF_ID, CONF_IP_ADDRESS, CONF_NAME, CONF_SSID
from esphome.cpp_generator import Pvariable
from esphome.cpp_types import App, Component

DEPENDENCIES = ['wifi']

IPAddressWiFiInfo = text_sensor.text_sensor_ns.class_('IPAddressWiFiInfo',
                                                      text_sensor.TextSensor, Component)
SSIDWiFiInfo = text_sensor.text_sensor_ns.class_('SSIDWiFiInfo',
                                                 text_sensor.TextSensor, Component)
BSSIDWiFiInfo = text_sensor.text_sensor_ns.class_('BSSIDWiFiInfo',
                                                  text_sensor.TextSensor, Component)

PLATFORM_SCHEMA = text_sensor.PLATFORM_SCHEMA.extend({
    vol.Optional(CONF_IP_ADDRESS): cv.nameable(text_sensor.TEXT_SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(IPAddressWiFiInfo),
    })),
    vol.Optional(CONF_SSID): cv.nameable(text_sensor.TEXT_SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(SSIDWiFiInfo),
    })),
    vol.Optional(CONF_BSSID): cv.nameable(text_sensor.TEXT_SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(BSSIDWiFiInfo),
    })),
})


def setup_conf(config, key, klass):
    if key in config:
        conf = config[key]
        rhs = App.register_component(klass.new(conf[CONF_NAME]))
        sensor_ = Pvariable(conf[CONF_ID], rhs)
        text_sensor.register_text_sensor(sensor_, conf)


def to_code(config):
    setup_conf(config, CONF_IP_ADDRESS, IPAddressWiFiInfo)
    setup_conf(config, CONF_SSID, SSIDWiFiInfo)
    setup_conf(config, CONF_BSSID, BSSIDWiFiInfo)


BUILD_FLAGS = '-DUSE_WIFI_INFO_TEXT_SENSOR'
