import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import core
from esphomeyaml.const import CONF_AP, CONF_CHANNEL, CONF_DNS1, CONF_DNS2, CONF_DOMAIN, \
    CONF_GATEWAY, CONF_HOSTNAME, CONF_ID, CONF_MANUAL_IP, CONF_PASSWORD, CONF_SSID, \
    CONF_STATIC_IP, CONF_SUBNET, ESP_PLATFORM_ESP8266
from esphomeyaml.helpers import App, Pvariable, StructInitializer, add, esphomelib_ns, global_ns


def validate_password(value):
    value = cv.string(value)
    if not value:
        return value
    if len(value) < 8:
        raise vol.Invalid(u"WPA password must be at least 8 characters long")
    if len(value) > 63:
        raise vol.Invalid(u"WPA password must be at most 63 characters long")
    return value


AP_MANUAL_IP_SCHEMA = vol.Schema({
    vol.Required(CONF_STATIC_IP): cv.ipv4,
    vol.Required(CONF_GATEWAY): cv.ipv4,
    vol.Required(CONF_SUBNET): cv.ipv4,
})

STA_MANUAL_IP_SCHEMA = AP_MANUAL_IP_SCHEMA.extend({
    vol.Inclusive(CONF_DNS1, 'dns'): cv.ipv4,
    vol.Inclusive(CONF_DNS2, 'dns'): cv.ipv4,
})

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID('wifi'): cv.register_variable_id,
    vol.Optional(CONF_SSID): cv.ssid,
    vol.Optional(CONF_PASSWORD): validate_password,
    vol.Optional(CONF_MANUAL_IP): STA_MANUAL_IP_SCHEMA,
    vol.Optional(CONF_AP): vol.Schema({
        vol.Required(CONF_SSID): cv.ssid,
        vol.Optional(CONF_PASSWORD): validate_password,
        vol.Optional(CONF_CHANNEL): vol.All(cv.positive_int, vol.Range(min=1, max=14)),
        vol.Optional(CONF_MANUAL_IP): AP_MANUAL_IP_SCHEMA,
    }),
    vol.Optional(CONF_HOSTNAME): cv.hostname,
    vol.Required(CONF_DOMAIN, default='.local'): cv.domainname,
})

# pylint: disable=invalid-name
IPAddress = global_ns.IPAddress
ManualIP = esphomelib_ns.ManualIP
WiFiComponent = esphomelib_ns.WiFiComponent


def safe_ip(ip):
    if ip is None:
        return None
    return IPAddress(*ip.args)


def manual_ip(config):
    return StructInitializer(
        ManualIP,
        ('static_ip', safe_ip(config[CONF_STATIC_IP])),
        ('gateway', safe_ip(config[CONF_GATEWAY])),
        ('subnet', safe_ip(config[CONF_SUBNET])),
        ('dns1', safe_ip(config.get(CONF_DNS1))),
        ('dns2', safe_ip(config.get(CONF_DNS2))),
    )


def to_code(config):
    sta = CONF_SSID in config
    ap = CONF_AP in config
    if sta:
        rhs = App.init_wifi(config[CONF_SSID], config.get(CONF_PASSWORD))
    else:
        rhs = App.init_wifi()
    wifi = Pvariable(WiFiComponent, config[CONF_ID], rhs)

    if sta and CONF_MANUAL_IP in config:
        add(wifi.set_sta_manual_ip(manual_ip(config[CONF_MANUAL_IP])))

    if ap:
        conf = config[CONF_AP]
        password = config.get(CONF_PASSWORD)
        if password is None and CONF_CHANNEL in conf:
            password = u""
        add(wifi.set_ap(conf[CONF_SSID], password, conf.get(CONF_CHANNEL)))

        if CONF_MANUAL_IP in conf:
            add(wifi.set_ap_manual_ip(manual_ip(conf[CONF_MANUAL_IP])))

    if CONF_HOSTNAME in config:
        add(wifi.set_hostname(config[CONF_HOSTNAME]))


def lib_deps(config):
    if core.ESP_PLATFORM == ESP_PLATFORM_ESP8266:
        return 'ESP8266WiFi'
    return None
