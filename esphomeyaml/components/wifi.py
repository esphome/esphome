import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_DNS1, CONF_DNS2, CONF_GATEWAY, CONF_HOSTNAME, CONF_ID, \
    CONF_MANUAL_IP, CONF_PASSWORD, CONF_SSID, CONF_STATIC_IP, CONF_SUBNET, CONF_WIFI
from esphomeyaml.helpers import App, MockObj, Pvariable, StructInitializer, add

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(CONF_WIFI): cv.register_variable_id,
    vol.Required(CONF_SSID): cv.ssid,
    vol.Optional(CONF_PASSWORD): cv.string,
    vol.Optional(CONF_MANUAL_IP): vol.Schema({
        vol.Required(CONF_STATIC_IP): cv.ipv4,
        vol.Required(CONF_GATEWAY): cv.ipv4,
        vol.Required(CONF_SUBNET): cv.ipv4,
        vol.Inclusive(CONF_DNS1, 'dns'): cv.ipv4,
        vol.Inclusive(CONF_DNS2, 'dns'): cv.ipv4,
    }),
    vol.Optional(CONF_HOSTNAME): cv.hostname,
})


# pylint: disable=invalid-name
IPAddress = MockObj('IPAddress')


def safe_ip(ip):
    if ip is None:
        return None
    return IPAddress(*ip.args)


def to_code(config):
    rhs = App.init_wifi(config[CONF_SSID], config.get(CONF_PASSWORD))
    wifi = Pvariable('WiFiComponent', config[CONF_ID], rhs)
    if CONF_MANUAL_IP in config:
        manual_ip = config[CONF_MANUAL_IP]
        exp = StructInitializer(
            'ManualIP',
            ('static_ip', safe_ip(manual_ip[CONF_STATIC_IP])),
            ('gateway', safe_ip(manual_ip[CONF_GATEWAY])),
            ('subnet', safe_ip(manual_ip[CONF_SUBNET])),
            ('dns1', safe_ip(manual_ip.get(CONF_DNS1))),
            ('dns2', safe_ip(manual_ip.get(CONF_DNS2))),
        )
        add(wifi.set_manual_ip(exp))
    if CONF_HOSTNAME in config:
        add(wifi.set_hostname(config[CONF_HOSTNAME]))
