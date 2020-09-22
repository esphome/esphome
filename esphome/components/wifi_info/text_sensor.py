import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_BSSID, CONF_ID, CONF_IP_ADDRESS, CONF_SSID, CONF_MAC_ADDRESS
from esphome.core import coroutine

DEPENDENCIES = ['wifi']

wifi_info_ns = cg.esphome_ns.namespace('wifi_info')
IPAddressWiFiInfo = wifi_info_ns.class_('IPAddressWiFiInfo', text_sensor.TextSensor, cg.Component)
SSIDWiFiInfo = wifi_info_ns.class_('SSIDWiFiInfo', text_sensor.TextSensor, cg.Component)
BSSIDWiFiInfo = wifi_info_ns.class_('BSSIDWiFiInfo', text_sensor.TextSensor, cg.Component)
MacAddressWifiInfo = wifi_info_ns.class_('MacAddressWifiInfo', text_sensor.TextSensor, cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.Optional(CONF_IP_ADDRESS): text_sensor.TEXT_SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(IPAddressWiFiInfo),
    }),
    cv.Optional(CONF_SSID): text_sensor.TEXT_SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(SSIDWiFiInfo),
    }),
    cv.Optional(CONF_BSSID): text_sensor.TEXT_SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(BSSIDWiFiInfo),
    }),
    cv.Optional(CONF_MAC_ADDRESS): text_sensor.TEXT_SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(MacAddressWifiInfo),
    })
})


@coroutine
def setup_conf(config, key):
    if key in config:
        conf = config[key]
        var = cg.new_Pvariable(conf[CONF_ID])
        yield cg.register_component(var, conf)
        yield text_sensor.register_text_sensor(var, conf)


def to_code(config):
    yield setup_conf(config, CONF_IP_ADDRESS)
    yield setup_conf(config, CONF_SSID)
    yield setup_conf(config, CONF_BSSID)
    yield setup_conf(config, CONF_MAC_ADDRESS)
