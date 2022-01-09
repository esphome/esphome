import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome import automation
from esphome.automation import Condition
from esphome.const import (
    CONF_AP,
    CONF_BSSID,
    CONF_CHANNEL,
    CONF_DNS1,
    CONF_DNS2,
    CONF_DOMAIN,
    CONF_FAST_CONNECT,
    CONF_GATEWAY,
    CONF_HIDDEN,
    CONF_ID,
    CONF_MANUAL_IP,
    CONF_NETWORKS,
    CONF_PASSWORD,
    CONF_POWER_SAVE_MODE,
    CONF_REBOOT_TIMEOUT,
    CONF_SSID,
    CONF_STATIC_IP,
    CONF_SUBNET,
    CONF_USE_ADDRESS,
    CONF_PRIORITY,
    CONF_IDENTITY,
    CONF_CERTIFICATE_AUTHORITY,
    CONF_CERTIFICATE,
    CONF_KEY,
    CONF_USERNAME,
    CONF_EAP,
)
from esphome.core import CORE, HexInt, coroutine_with_priority
from esphome.components.network import IPAddress
from . import wpa2_eap


AUTO_LOAD = ["network"]

wifi_ns = cg.esphome_ns.namespace("wifi")
EAPAuth = wifi_ns.struct("EAPAuth")
ManualIP = wifi_ns.struct("ManualIP")
WiFiComponent = wifi_ns.class_("WiFiComponent", cg.Component)
WiFiAP = wifi_ns.struct("WiFiAP")

WiFiPowerSaveMode = wifi_ns.enum("WiFiPowerSaveMode")
WIFI_POWER_SAVE_MODES = {
    "NONE": WiFiPowerSaveMode.WIFI_POWER_SAVE_NONE,
    "LIGHT": WiFiPowerSaveMode.WIFI_POWER_SAVE_LIGHT,
    "HIGH": WiFiPowerSaveMode.WIFI_POWER_SAVE_HIGH,
}
WiFiConnectedCondition = wifi_ns.class_("WiFiConnectedCondition", Condition)


def validate_password(value):
    value = cv.string_strict(value)
    if not value:
        return value
    if len(value) < 8:
        raise cv.Invalid("WPA password must be at least 8 characters long")
    if len(value) > 64:
        raise cv.Invalid("WPA password must be at most 64 characters long")
    return value


def validate_channel(value):
    value = cv.positive_int(value)
    if value < 1:
        raise cv.Invalid("Minimum WiFi channel is 1")
    if value > 14:
        raise cv.Invalid("Maximum WiFi channel is 14")
    return value


AP_MANUAL_IP_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_STATIC_IP): cv.ipv4,
        cv.Required(CONF_GATEWAY): cv.ipv4,
        cv.Required(CONF_SUBNET): cv.ipv4,
    }
)

STA_MANUAL_IP_SCHEMA = AP_MANUAL_IP_SCHEMA.extend(
    {
        cv.Optional(CONF_DNS1, default="0.0.0.0"): cv.ipv4,
        cv.Optional(CONF_DNS2, default="0.0.0.0"): cv.ipv4,
    }
)

EAP_AUTH_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_IDENTITY): cv.string_strict,
            cv.Optional(CONF_USERNAME): cv.string_strict,
            cv.Optional(CONF_PASSWORD): cv.string_strict,
            cv.Optional(CONF_CERTIFICATE_AUTHORITY): wpa2_eap.validate_certificate,
            cv.Inclusive(
                CONF_CERTIFICATE, "certificate_and_key"
            ): wpa2_eap.validate_certificate,
            # Only validate as file first because we need the password to load it
            # Actual validation happens in validate_eap.
            cv.Inclusive(CONF_KEY, "certificate_and_key"): cv.file_,
        }
    ),
    wpa2_eap.validate_eap,
    cv.has_at_least_one_key(CONF_IDENTITY, CONF_CERTIFICATE),
)

WIFI_NETWORK_BASE = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WiFiAP),
        cv.Optional(CONF_SSID): cv.ssid,
        cv.Optional(CONF_PASSWORD): validate_password,
        cv.Optional(CONF_CHANNEL): validate_channel,
        cv.Optional(CONF_MANUAL_IP): STA_MANUAL_IP_SCHEMA,
    }
)

CONF_AP_TIMEOUT = "ap_timeout"
WIFI_NETWORK_AP = WIFI_NETWORK_BASE.extend(
    {
        cv.Optional(
            CONF_AP_TIMEOUT, default="1min"
        ): cv.positive_time_period_milliseconds,
    }
)

WIFI_NETWORK_STA = WIFI_NETWORK_BASE.extend(
    {
        cv.Optional(CONF_BSSID): cv.mac_address,
        cv.Optional(CONF_HIDDEN): cv.boolean,
        cv.Optional(CONF_PRIORITY, default=0.0): cv.float_,
        cv.Optional(CONF_EAP): EAP_AUTH_SCHEMA,
    }
)


def final_validate(config):
    has_sta = bool(config.get(CONF_NETWORKS, True))
    has_ap = CONF_AP in config
    has_improv = "esp32_improv" in fv.full_config.get()
    has_improv_serial = "improv_serial" in fv.full_config.get()
    if not (has_sta or has_ap or has_improv or has_improv_serial):
        raise cv.Invalid(
            "Please specify at least an SSID or an Access Point to create."
        )


def final_validate_power_esp32_ble(value):
    if not CORE.is_esp32:
        return
    if value != "NONE":
        # WiFi should be in modem sleep (!=NONE) with BLE coexistence
        # https://docs.espressif.com/projects/esp-idf/en/v3.3.5/api-guides/wifi.html#station-sleep
        return
    for conflicting in [
        "esp32_ble",
        "esp32_ble_beacon",
        "esp32_ble_server",
        "esp32_ble_tracker",
    ]:
        if conflicting not in fv.full_config.get():
            continue

        try:
            # Only arduino 1.0.5+ and esp-idf impacted
            cv.require_framework_version(
                esp32_arduino=cv.Version(1, 0, 5),
                esp_idf=cv.Version(4, 0, 0),
            )(None)
        except cv.Invalid:
            pass
        else:
            raise cv.Invalid(
                f"power_save_mode NONE is incompatible with {conflicting}. "
                f"Please remove the power save mode. See also "
                f"https://github.com/esphome/issues/issues/2141#issuecomment-865688582"
            )


FINAL_VALIDATE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_POWER_SAVE_MODE): final_validate_power_esp32_ble,
        },
        extra=cv.ALLOW_EXTRA,
    ),
    final_validate,
)


def _validate(config):
    if CONF_PASSWORD in config and CONF_SSID not in config:
        raise cv.Invalid("Cannot have WiFi password without SSID!")

    if CONF_SSID in config:
        # Automatically move single network to 'networks' section
        config = config.copy()
        network = {CONF_SSID: config.pop(CONF_SSID)}
        if CONF_PASSWORD in config:
            network[CONF_PASSWORD] = config.pop(CONF_PASSWORD)
        if CONF_EAP in config:
            network[CONF_EAP] = config.pop(CONF_EAP)
        if CONF_NETWORKS in config:
            raise cv.Invalid(
                "You cannot use the 'ssid:' option together with 'networks:'. Please "
                "copy your network into the 'networks:' key"
            )
        config[CONF_NETWORKS] = cv.ensure_list(WIFI_NETWORK_STA)(network)

    if (CONF_NETWORKS not in config) and (CONF_AP not in config):
        config = config.copy()
        config[CONF_NETWORKS] = []

    if config.get(CONF_FAST_CONNECT, False):
        networks = config.get(CONF_NETWORKS, [])
        if not networks:
            raise cv.Invalid("At least one network required for fast_connect!")
        if len(networks) != 1:
            raise cv.Invalid("Fast connect can only be used with one network!")

    if CONF_USE_ADDRESS not in config:
        use_address = CORE.name + config[CONF_DOMAIN]
        if CONF_MANUAL_IP in config:
            use_address = str(config[CONF_MANUAL_IP][CONF_STATIC_IP])
        elif CONF_NETWORKS in config:
            ips = set(
                str(net[CONF_MANUAL_IP][CONF_STATIC_IP])
                for net in config[CONF_NETWORKS]
                if CONF_MANUAL_IP in net
            )
            if len(ips) > 1:
                raise cv.Invalid(
                    "Must specify use_address when using multiple static IP addresses."
                )
            if len(ips) == 1:
                use_address = next(iter(ips))

        config[CONF_USE_ADDRESS] = use_address

    return config


CONF_OUTPUT_POWER = "output_power"
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WiFiComponent),
            cv.Optional(CONF_NETWORKS): cv.ensure_list(WIFI_NETWORK_STA),
            cv.Optional(CONF_SSID): cv.ssid,
            cv.Optional(CONF_PASSWORD): validate_password,
            cv.Optional(CONF_MANUAL_IP): STA_MANUAL_IP_SCHEMA,
            cv.Optional(CONF_EAP): EAP_AUTH_SCHEMA,
            cv.Optional(CONF_AP): WIFI_NETWORK_AP,
            cv.Optional(CONF_DOMAIN, default=".local"): cv.domain_name,
            cv.Optional(
                CONF_REBOOT_TIMEOUT, default="15min"
            ): cv.positive_time_period_milliseconds,
            cv.SplitDefault(
                CONF_POWER_SAVE_MODE, esp8266="none", esp32="light"
            ): cv.enum(WIFI_POWER_SAVE_MODES, upper=True),
            cv.Optional(CONF_FAST_CONNECT, default=False): cv.boolean,
            cv.Optional(CONF_USE_ADDRESS): cv.string_strict,
            cv.SplitDefault(CONF_OUTPUT_POWER, esp8266=20.0): cv.All(
                cv.decibel, cv.float_range(min=10.0, max=20.5)
            ),
            cv.Optional("enable_mdns"): cv.invalid(
                "This option has been removed. Please use the [disabled] option under the "
                "new mdns component instead."
            ),
        }
    ),
    _validate,
)


def eap_auth(config):
    if config is None:
        return None
    ca_cert = ""
    if CONF_CERTIFICATE_AUTHORITY in config:
        ca_cert = wpa2_eap.read_relative_config_path(config[CONF_CERTIFICATE_AUTHORITY])
    client_cert = ""
    if CONF_CERTIFICATE in config:
        client_cert = wpa2_eap.read_relative_config_path(config[CONF_CERTIFICATE])
    key = ""
    if CONF_KEY in config:
        key = wpa2_eap.read_relative_config_path(config[CONF_KEY])
    return cg.StructInitializer(
        EAPAuth,
        ("identity", config.get(CONF_IDENTITY, "")),
        ("username", config.get(CONF_USERNAME, "")),
        ("password", config.get(CONF_PASSWORD, "")),
        ("ca_cert", ca_cert),
        ("client_cert", client_cert),
        ("client_key", key),
    )


def safe_ip(ip):
    if ip is None:
        return IPAddress(0, 0, 0, 0)
    return IPAddress(*ip.args)


def manual_ip(config):
    if config is None:
        return None
    return cg.StructInitializer(
        ManualIP,
        ("static_ip", safe_ip(config[CONF_STATIC_IP])),
        ("gateway", safe_ip(config[CONF_GATEWAY])),
        ("subnet", safe_ip(config[CONF_SUBNET])),
        ("dns1", safe_ip(config.get(CONF_DNS1))),
        ("dns2", safe_ip(config.get(CONF_DNS2))),
    )


def wifi_network(config, static_ip):
    ap = cg.variable(config[CONF_ID], WiFiAP())
    if CONF_SSID in config:
        cg.add(ap.set_ssid(config[CONF_SSID]))
    if CONF_PASSWORD in config:
        cg.add(ap.set_password(config[CONF_PASSWORD]))
    if CONF_EAP in config:
        cg.add(ap.set_eap(eap_auth(config[CONF_EAP])))
        cg.add_define("USE_WIFI_WPA2_EAP")
    if CONF_BSSID in config:
        cg.add(ap.set_bssid([HexInt(i) for i in config[CONF_BSSID].parts]))
    if CONF_HIDDEN in config:
        cg.add(ap.set_hidden(config[CONF_HIDDEN]))
    if CONF_CHANNEL in config:
        cg.add(ap.set_channel(config[CONF_CHANNEL]))
    if static_ip is not None:
        cg.add(ap.set_manual_ip(manual_ip(static_ip)))
    if CONF_PRIORITY in config:
        cg.add(ap.set_priority(config[CONF_PRIORITY]))

    return ap


@coroutine_with_priority(60.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_use_address(config[CONF_USE_ADDRESS]))

    for network in config.get(CONF_NETWORKS, []):
        ip_config = network.get(CONF_MANUAL_IP, config.get(CONF_MANUAL_IP))
        cg.add(var.add_sta(wifi_network(network, ip_config)))

    if CONF_AP in config:
        conf = config[CONF_AP]
        ip_config = conf.get(CONF_MANUAL_IP, config.get(CONF_MANUAL_IP))
        cg.add(var.set_ap(wifi_network(conf, ip_config)))
        cg.add(var.set_ap_timeout(conf[CONF_AP_TIMEOUT]))

    cg.add(var.set_reboot_timeout(config[CONF_REBOOT_TIMEOUT]))
    cg.add(var.set_power_save_mode(config[CONF_POWER_SAVE_MODE]))
    cg.add(var.set_fast_connect(config[CONF_FAST_CONNECT]))
    if CONF_OUTPUT_POWER in config:
        cg.add(var.set_output_power(config[CONF_OUTPUT_POWER]))

    if CORE.is_esp8266:
        cg.add_library("ESP8266WiFi", None)
    elif CORE.is_esp32 and CORE.using_arduino:
        cg.add_library("WiFi", None)

    cg.add_define("USE_WIFI")

    # Register at end for OTA safe mode
    await cg.register_component(var, config)


@automation.register_condition("wifi.connected", WiFiConnectedCondition, cv.Schema({}))
async def wifi_connected_to_code(config, condition_id, template_arg, args):
    return cg.new_Pvariable(condition_id, template_arg)
