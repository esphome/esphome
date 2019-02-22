import voluptuous as vol

import esphome.config_validation as cv
from esphome.const import CONF_AP, CONF_BSSID, CONF_CHANNEL, CONF_DNS1, CONF_DNS2, \
    CONF_DOMAIN, CONF_FAST_CONNECT, CONF_GATEWAY, CONF_ID, CONF_MANUAL_IP, CONF_NETWORKS, \
    CONF_PASSWORD, CONF_POWER_SAVE_MODE, CONF_REBOOT_TIMEOUT, CONF_SSID, CONF_STATIC_IP, \
    CONF_SUBNET, CONF_USE_ADDRESS, CONF_HIDDEN
from esphome.core import CORE, HexInt
from esphome.cpp_generator import Pvariable, StructInitializer, add, variable
from esphome.cpp_types import App, Component, esphome_ns, global_ns

IPAddress = global_ns.class_('IPAddress')
ManualIP = esphome_ns.struct('ManualIP')
WiFiComponent = esphome_ns.class_('WiFiComponent', Component)
WiFiAP = esphome_ns.struct('WiFiAP')

WiFiPowerSaveMode = esphome_ns.enum('WiFiPowerSaveMode')
WIFI_POWER_SAVE_MODES = {
    'NONE': WiFiPowerSaveMode.WIFI_POWER_SAVE_NONE,
    'LIGHT': WiFiPowerSaveMode.WIFI_POWER_SAVE_LIGHT,
    'HIGH': WiFiPowerSaveMode.WIFI_POWER_SAVE_HIGH,
}


def validate_password(value):
    value = cv.string_strict(value)
    if not value:
        return value
    if len(value) < 8:
        raise vol.Invalid(u"WPA password must be at least 8 characters long")
    if len(value) > 64:
        raise vol.Invalid(u"WPA password must be at most 64 characters long")
    return value


def validate_channel(value):
    value = cv.positive_int(value)
    if value < 1:
        raise vol.Invalid("Minimum WiFi channel is 1")
    if value > 14:
        raise vol.Invalid("Maximum WiFi channel is 14")
    return value


AP_MANUAL_IP_SCHEMA = vol.Schema({
    vol.Required(CONF_STATIC_IP): cv.ipv4,
    vol.Required(CONF_GATEWAY): cv.ipv4,
    vol.Required(CONF_SUBNET): cv.ipv4,
})

STA_MANUAL_IP_SCHEMA = AP_MANUAL_IP_SCHEMA.extend({
    vol.Optional(CONF_DNS1, default="1.1.1.1"): cv.ipv4,
    vol.Optional(CONF_DNS2, default="1.0.0.1"): cv.ipv4,
})

WIFI_NETWORK_BASE = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(WiFiAP),
    vol.Optional(CONF_SSID): cv.ssid,
    vol.Optional(CONF_PASSWORD): validate_password,
    vol.Optional(CONF_CHANNEL): validate_channel,
    vol.Optional(CONF_MANUAL_IP): STA_MANUAL_IP_SCHEMA,
})

WIFI_NETWORK_AP = WIFI_NETWORK_BASE.extend({

})

WIFI_NETWORK_STA = WIFI_NETWORK_BASE.extend({
    vol.Optional(CONF_BSSID): cv.mac_address,
    vol.Optional(CONF_HIDDEN): cv.boolean,
})


def validate(config):
    if CONF_PASSWORD in config and CONF_SSID not in config:
        raise vol.Invalid("Cannot have WiFi password without SSID!")

    if CONF_SSID in config:
        network = {CONF_SSID: config.pop(CONF_SSID)}
        if CONF_PASSWORD in config:
            network[CONF_PASSWORD] = config.pop(CONF_PASSWORD)
        if CONF_NETWORKS in config:
            raise vol.Invalid("You cannot use the 'ssid:' option together with 'networks:'. Please "
                              "copy your network into the 'networks:' key")
        config[CONF_NETWORKS] = cv.ensure_list(WIFI_NETWORK_STA)(network)

    if (CONF_NETWORKS not in config) and (CONF_AP not in config):
        raise vol.Invalid("Please specify at least an SSID or an Access Point "
                          "to create.")

    if config.get(CONF_FAST_CONNECT, False):
        networks = config.get(CONF_NETWORKS, [])
        if not networks:
            raise vol.Invalid("At least one network required for fast_connect!")
        if len(networks) != 1:
            raise vol.Invalid("Fast connect can only be used with one network!")

    if CONF_USE_ADDRESS not in config:
        if CONF_MANUAL_IP in config:
            use_address = str(config[CONF_MANUAL_IP][CONF_STATIC_IP])
        else:
            use_address = CORE.name + config[CONF_DOMAIN]
        config[CONF_USE_ADDRESS] = use_address

    return config


CONFIG_SCHEMA = vol.All(vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(WiFiComponent),
    vol.Optional(CONF_NETWORKS): cv.ensure_list(WIFI_NETWORK_STA),

    vol.Optional(CONF_SSID): cv.ssid,
    vol.Optional(CONF_PASSWORD): validate_password,
    vol.Optional(CONF_MANUAL_IP): STA_MANUAL_IP_SCHEMA,

    vol.Optional(CONF_AP): WIFI_NETWORK_AP,
    vol.Optional(CONF_DOMAIN, default='.local'): cv.domain_name,
    vol.Optional(CONF_REBOOT_TIMEOUT): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_POWER_SAVE_MODE): cv.one_of(*WIFI_POWER_SAVE_MODES, upper=True),
    vol.Optional(CONF_FAST_CONNECT): cv.boolean,
    vol.Optional(CONF_USE_ADDRESS): cv.string_strict,

    vol.Optional('hostname'): cv.invalid("The hostname option has been removed in 1.11.0"),
}), validate)


def safe_ip(ip):
    if ip is None:
        return IPAddress(0, 0, 0, 0)
    return IPAddress(*ip.args)


def manual_ip(config):
    if config is None:
        return None
    return StructInitializer(
        ManualIP,
        ('static_ip', safe_ip(config[CONF_STATIC_IP])),
        ('gateway', safe_ip(config[CONF_GATEWAY])),
        ('subnet', safe_ip(config[CONF_SUBNET])),
        ('dns1', safe_ip(config.get(CONF_DNS1))),
        ('dns2', safe_ip(config.get(CONF_DNS2))),
    )


def wifi_network(config, static_ip):
    ap = variable(config[CONF_ID], WiFiAP())
    if CONF_SSID in config:
        add(ap.set_ssid(config[CONF_SSID]))
    if CONF_PASSWORD in config:
        add(ap.set_password(config[CONF_PASSWORD]))
    if CONF_BSSID in config:
        add(ap.set_bssid([HexInt(i) for i in config[CONF_BSSID].parts]))
    if CONF_HIDDEN in config:
        add(ap.set_hidden(config[CONF_HIDDEN]))
    if CONF_CHANNEL in config:
        add(ap.set_channel(config[CONF_CHANNEL]))
    if static_ip is not None:
        add(ap.set_manual_ip(manual_ip(static_ip)))

    return ap


def to_code(config):
    rhs = App.init_wifi()
    wifi = Pvariable(config[CONF_ID], rhs)
    add(wifi.set_use_address(config[CONF_USE_ADDRESS]))

    for network in config.get(CONF_NETWORKS, []):
        add(wifi.add_sta(wifi_network(network, config.get(CONF_MANUAL_IP))))

    if CONF_AP in config:
        add(wifi.set_ap(wifi_network(config[CONF_AP], config.get(CONF_MANUAL_IP))))

    if CONF_REBOOT_TIMEOUT in config:
        add(wifi.set_reboot_timeout(config[CONF_REBOOT_TIMEOUT]))

    if CONF_POWER_SAVE_MODE in config:
        add(wifi.set_power_save_mode(WIFI_POWER_SAVE_MODES[config[CONF_POWER_SAVE_MODE]]))

    if CONF_FAST_CONNECT in config:
        add(wifi.set_fast_connect(config[CONF_FAST_CONNECT]))


def lib_deps(config):
    if CORE.is_esp8266:
        return 'ESP8266WiFi'
    if CORE.is_esp32:
        return None
    raise NotImplementedError
