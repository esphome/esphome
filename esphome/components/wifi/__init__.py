import logging

from esphome import automation
from esphome.automation import Condition
import esphome.codegen as cg
from esphome.components.const import CONF_USE_PSRAM
from esphome.components.esp32 import add_idf_sdkconfig_option, const, get_esp32_variant
from esphome.components.network import (
    has_high_performance_networking,
    ip_address_literal,
)
from esphome.components.psram import is_guaranteed as psram_is_guaranteed
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_AP,
    CONF_BSSID,
    CONF_CERTIFICATE,
    CONF_CERTIFICATE_AUTHORITY,
    CONF_CHANNEL,
    CONF_DNS1,
    CONF_DNS2,
    CONF_DOMAIN,
    CONF_EAP,
    CONF_ENABLE_BTM,
    CONF_ENABLE_ON_BOOT,
    CONF_ENABLE_RRM,
    CONF_FAST_CONNECT,
    CONF_GATEWAY,
    CONF_HIDDEN,
    CONF_ID,
    CONF_IDENTITY,
    CONF_KEY,
    CONF_MANUAL_IP,
    CONF_NETWORKS,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_ON_ERROR,
    CONF_PASSWORD,
    CONF_POWER_SAVE_MODE,
    CONF_PRIORITY,
    CONF_REBOOT_TIMEOUT,
    CONF_SSID,
    CONF_STATIC_IP,
    CONF_SUBNET,
    CONF_TIMEOUT,
    CONF_TTLS_PHASE_2,
    CONF_USE_ADDRESS,
    CONF_USERNAME,
    Platform,
    PlatformFramework,
)
from esphome.core import CORE, CoroPriority, HexInt, coroutine_with_priority
import esphome.final_validate as fv

from . import wpa2_eap

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["network"]

_LOGGER = logging.getLogger(__name__)

NO_WIFI_VARIANTS = [const.VARIANT_ESP32H2, const.VARIANT_ESP32P4]
CONF_SAVE = "save"
CONF_MIN_AUTH_MODE = "min_auth_mode"

# Maximum number of WiFi networks that can be configured
# Limited to 127 because selected_sta_index_ is int8_t in C++
MAX_WIFI_NETWORKS = 127

# Default AP timeout - allows sufficient time to try all BSSIDs during initial connection
# After AP starts, WiFi scanning is skipped to avoid disrupting the AP, so we only
# get best-effort connection attempts. Longer timeout ensures we exhaust all options
# before falling back to AP mode. Aligned with improv wifi_timeout default.
DEFAULT_AP_TIMEOUT = "90s"

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

WifiMinAuthMode = wifi_ns.enum("WifiMinAuthMode")
WIFI_MIN_AUTH_MODES = {
    "WPA": WifiMinAuthMode.WIFI_MIN_AUTH_MODE_WPA,
    "WPA2": WifiMinAuthMode.WIFI_MIN_AUTH_MODE_WPA2,
    "WPA3": WifiMinAuthMode.WIFI_MIN_AUTH_MODE_WPA3,
}
VALIDATE_WIFI_MIN_AUTH_MODE = cv.enum(WIFI_MIN_AUTH_MODES, upper=True)
WiFiConnectedCondition = wifi_ns.class_("WiFiConnectedCondition", Condition)
WiFiEnabledCondition = wifi_ns.class_("WiFiEnabledCondition", Condition)
WiFiAPActiveCondition = wifi_ns.class_("WiFiAPActiveCondition", Condition)
WiFiEnableAction = wifi_ns.class_("WiFiEnableAction", automation.Action)
WiFiDisableAction = wifi_ns.class_("WiFiDisableAction", automation.Action)
WiFiConfigureAction = wifi_ns.class_(
    "WiFiConfigureAction", automation.Action, cg.Component
)


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
        cv.Required(CONF_STATIC_IP): cv.ipv4address,
        cv.Required(CONF_GATEWAY): cv.ipv4address,
        cv.Required(CONF_SUBNET): cv.ipv4address,
    }
)

STA_MANUAL_IP_SCHEMA = AP_MANUAL_IP_SCHEMA.extend(
    {
        cv.Optional(CONF_DNS1, default="0.0.0.0"): cv.ipv4address,
        cv.Optional(CONF_DNS2, default="0.0.0.0"): cv.ipv4address,
    }
)

TTLS_PHASE_2 = {
    "pap": cg.global_ns.ESP_EAP_TTLS_PHASE2_PAP,
    "chap": cg.global_ns.ESP_EAP_TTLS_PHASE2_CHAP,
    "mschap": cg.global_ns.ESP_EAP_TTLS_PHASE2_MSCHAP,
    "mschapv2": cg.global_ns.ESP_EAP_TTLS_PHASE2_MSCHAPV2,
    "eap": cg.global_ns.ESP_EAP_TTLS_PHASE2_EAP,
}

EAP_AUTH_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_IDENTITY): cv.string_strict,
            cv.Optional(CONF_USERNAME): cv.string_strict,
            cv.Optional(CONF_PASSWORD): cv.string_strict,
            cv.Optional(CONF_CERTIFICATE_AUTHORITY): wpa2_eap.validate_certificate,
            cv.SplitDefault(CONF_TTLS_PHASE_2, esp32="mschapv2"): cv.All(
                cv.enum(TTLS_PHASE_2), cv.only_on_esp32
            ),
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
            CONF_AP_TIMEOUT, default=DEFAULT_AP_TIMEOUT
        ): cv.positive_time_period_milliseconds,
    }
)


def wifi_network_ap(value):
    if value is None:
        value = {}
    return WIFI_NETWORK_AP(value)


WIFI_NETWORK_STA = WIFI_NETWORK_BASE.extend(
    {
        cv.Optional(CONF_BSSID): cv.mac_address,
        cv.Optional(CONF_HIDDEN): cv.boolean,
        cv.Optional(CONF_PRIORITY, default=0): cv.int_range(min=-128, max=127),
        cv.Optional(CONF_EAP): EAP_AUTH_SCHEMA,
    }
)


def validate_variant(_):
    if CORE.is_esp32:
        variant = get_esp32_variant()
        if variant in NO_WIFI_VARIANTS and "esp32_hosted" not in fv.full_config.get():
            raise cv.Invalid(f"WiFi requires component esp32_hosted on {variant}")


def _apply_min_auth_mode_default(config):
    """Apply platform-specific default for min_auth_mode and warn ESP8266 users."""
    # Only apply defaults for platforms that support min_auth_mode
    if CONF_MIN_AUTH_MODE not in config and (CORE.is_esp8266 or CORE.is_esp32):
        if CORE.is_esp8266:
            _LOGGER.warning(
                "The minimum WiFi authentication mode (wifi -> min_auth_mode) is not set. "
                "This controls the weakest encryption your device will accept when connecting to WiFi. "
                "Currently defaults to WPA (less secure), but will change to WPA2 (more secure) in 2026.6.0. "
                "WPA uses TKIP encryption which has known security vulnerabilities and should be avoided. "
                "WPA2 uses AES encryption which is significantly more secure. "
                "To silence this warning, explicitly set min_auth_mode under 'wifi:'. "
                "If your router supports WPA2 or WPA3, set 'min_auth_mode: WPA2'. "
                "If your router only supports WPA, set 'min_auth_mode: WPA'."
            )
            config[CONF_MIN_AUTH_MODE] = VALIDATE_WIFI_MIN_AUTH_MODE("WPA")
        elif CORE.is_esp32:
            config[CONF_MIN_AUTH_MODE] = VALIDATE_WIFI_MIN_AUTH_MODE("WPA2")
    return config


def final_validate(config):
    has_sta = bool(config.get(CONF_NETWORKS, True))
    has_ap = CONF_AP in config
    has_improv = "esp32_improv" in fv.full_config.get()
    has_improv_serial = "improv_serial" in fv.full_config.get()
    if not (has_sta or has_ap or has_improv or has_improv_serial):
        raise cv.Invalid(
            "Please specify at least an SSID or an Access Point to create."
        )


FINAL_VALIDATE_SCHEMA = cv.All(
    final_validate,
    validate_variant,
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
            # In testing mode, merged component tests may have both ssid and networks
            # Just use the networks list and ignore the single ssid
            if not CORE.testing_mode:
                raise cv.Invalid(
                    "You cannot use the 'ssid:' option together with 'networks:'. Please "
                    "copy your network into the 'networks:' key"
                )
        else:
            config[CONF_NETWORKS] = cv.ensure_list(WIFI_NETWORK_STA)(network)

    if (CONF_NETWORKS not in config) and (CONF_AP not in config):
        config = config.copy()
        config[CONF_NETWORKS] = []

    if config.get(CONF_FAST_CONNECT, False):
        networks = config.get(CONF_NETWORKS, [])
        if not networks:
            raise cv.Invalid("At least one network required for fast_connect!")

    if CONF_USE_ADDRESS not in config:
        use_address = CORE.name + config[CONF_DOMAIN]
        if CONF_MANUAL_IP in config:
            use_address = str(config[CONF_MANUAL_IP][CONF_STATIC_IP])
        elif CONF_NETWORKS in config:
            ips = {
                str(net[CONF_MANUAL_IP][CONF_STATIC_IP])
                for net in config[CONF_NETWORKS]
                if CONF_MANUAL_IP in net
            }
            if len(ips) > 1:
                raise cv.Invalid(
                    "Must specify use_address when using multiple static IP addresses."
                )
            if len(ips) == 1:
                use_address = next(iter(ips))

        config[CONF_USE_ADDRESS] = use_address

    return config


CONF_OUTPUT_POWER = "output_power"
CONF_PASSIVE_SCAN = "passive_scan"
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WiFiComponent),
            cv.Optional(CONF_NETWORKS): cv.All(
                cv.ensure_list(WIFI_NETWORK_STA), cv.Length(max=MAX_WIFI_NETWORKS)
            ),
            cv.Optional(CONF_SSID): cv.ssid,
            cv.Optional(CONF_PASSWORD): validate_password,
            cv.Optional(CONF_MANUAL_IP): STA_MANUAL_IP_SCHEMA,
            cv.Optional(CONF_EAP): EAP_AUTH_SCHEMA,
            cv.Optional(CONF_AP): wifi_network_ap,
            cv.Optional(CONF_DOMAIN, default=".local"): cv.domain_name,
            cv.Optional(
                CONF_REBOOT_TIMEOUT, default="15min"
            ): cv.positive_time_period_milliseconds,
            cv.SplitDefault(
                CONF_POWER_SAVE_MODE,
                esp8266="none",
                esp32="light",
                rp2040="light",
                bk72xx="none",
                rtl87xx="none",
                ln882x="light",
            ): cv.enum(WIFI_POWER_SAVE_MODES, upper=True),
            cv.Optional(CONF_FAST_CONNECT, default=False): cv.boolean,
            cv.Optional(CONF_USE_ADDRESS): cv.string_strict,
            cv.Optional(CONF_MIN_AUTH_MODE): cv.All(
                VALIDATE_WIFI_MIN_AUTH_MODE,
                cv.only_on([Platform.ESP32, Platform.ESP8266]),
            ),
            cv.SplitDefault(CONF_OUTPUT_POWER, esp8266=20.0): cv.All(
                cv.decibel, cv.float_range(min=8.5, max=20.5)
            ),
            cv.SplitDefault(CONF_ENABLE_BTM, esp32=False): cv.All(
                cv.boolean, cv.only_on_esp32
            ),
            cv.SplitDefault(CONF_ENABLE_RRM, esp32=False): cv.All(
                cv.boolean, cv.only_on_esp32
            ),
            cv.Optional(CONF_PASSIVE_SCAN, default=False): cv.boolean,
            cv.Optional("enable_mdns"): cv.invalid(
                "This option has been removed. Please use the [disabled] option under the "
                "new mdns component instead."
            ),
            cv.Optional(CONF_ENABLE_ON_BOOT, default=True): cv.boolean,
            cv.Optional(CONF_ON_CONNECT): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_USE_PSRAM): cv.All(
                cv.only_on_esp32, cv.requires_component("psram"), cv.boolean
            ),
        }
    ),
    _apply_min_auth_mode_default,
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
        ("ttls_phase_2", config.get(CONF_TTLS_PHASE_2)),
    )


def safe_ip(ip):
    return ip_address_literal(ip)


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


def wifi_network(config, ap, static_ip):
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


@coroutine_with_priority(CoroPriority.COMMUNICATION)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_use_address(config[CONF_USE_ADDRESS]))

    # Track if any network uses Enterprise authentication
    has_eap = False
    # Track if any network uses manual IP
    has_manual_ip = False

    # Initialize FixedVector with the count of networks
    networks = config.get(CONF_NETWORKS, [])
    if networks:
        cg.add(var.init_sta(len(networks)))

        def add_sta(ap: cg.MockObj, network: dict) -> None:
            ip_config = network.get(CONF_MANUAL_IP, config.get(CONF_MANUAL_IP))
            cg.add(var.add_sta(wifi_network(network, ap, ip_config)))

        for network in networks:
            if CONF_EAP in network:
                has_eap = True
            if network.get(CONF_MANUAL_IP) or config.get(CONF_MANUAL_IP):
                has_manual_ip = True
            cg.with_local_variable(network[CONF_ID], WiFiAP(), add_sta, network)

    if CONF_AP in config:
        conf = config[CONF_AP]
        ip_config = conf.get(CONF_MANUAL_IP)
        if ip_config:
            has_manual_ip = True
        cg.with_local_variable(
            conf[CONF_ID],
            WiFiAP(),
            lambda ap: cg.add(var.set_ap(wifi_network(conf, ap, ip_config))),
        )
        cg.add(var.set_ap_timeout(conf[CONF_AP_TIMEOUT]))
        cg.add_define("USE_WIFI_AP")
    elif CORE.is_esp32 and CORE.using_esp_idf:
        add_idf_sdkconfig_option("CONFIG_ESP_WIFI_SOFTAP_SUPPORT", False)
        add_idf_sdkconfig_option("CONFIG_LWIP_DHCPS", False)

    # Disable Enterprise WiFi support if no EAP is configured
    if CORE.is_esp32:
        add_idf_sdkconfig_option("CONFIG_ESP_WIFI_ENTERPRISE_SUPPORT", has_eap)

    # Only define USE_WIFI_MANUAL_IP if any AP uses manual IP
    if has_manual_ip:
        cg.add_define("USE_WIFI_MANUAL_IP")

    cg.add(var.set_reboot_timeout(config[CONF_REBOOT_TIMEOUT]))
    cg.add(var.set_power_save_mode(config[CONF_POWER_SAVE_MODE]))
    if CONF_MIN_AUTH_MODE in config:
        cg.add(var.set_min_auth_mode(config[CONF_MIN_AUTH_MODE]))
    if config[CONF_FAST_CONNECT]:
        cg.add_define("USE_WIFI_FAST_CONNECT")
    # passive_scan defaults to false in C++ - only set if true
    if config[CONF_PASSIVE_SCAN]:
        cg.add(var.set_passive_scan(True))
    if CONF_OUTPUT_POWER in config:
        cg.add(var.set_output_power(config[CONF_OUTPUT_POWER]))
    # enable_on_boot defaults to true in C++ - only set if false
    if not config[CONF_ENABLE_ON_BOOT]:
        cg.add(var.set_enable_on_boot(False))

    if CORE.is_esp8266:
        cg.add_library("ESP8266WiFi", None)
    elif CORE.is_rp2040:
        cg.add_library("WiFi", None)

    if CORE.is_esp32:
        if config[CONF_ENABLE_BTM] or config[CONF_ENABLE_RRM]:
            add_idf_sdkconfig_option("CONFIG_WPA_11KV_SUPPORT", True)
            cg.add_define("USE_WIFI_11KV_SUPPORT")
        if config[CONF_ENABLE_BTM]:
            cg.add(var.set_btm(config[CONF_ENABLE_BTM]))
        if config[CONF_ENABLE_RRM]:
            cg.add(var.set_rrm(config[CONF_ENABLE_RRM]))

    if config.get(CONF_USE_PSRAM):
        add_idf_sdkconfig_option("CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP", True)

    # Apply high performance WiFi settings if high performance networking is enabled
    if CORE.is_esp32 and CORE.using_esp_idf and has_high_performance_networking():
        # Check if PSRAM is guaranteed (set by psram component during final validation)
        psram_guaranteed = psram_is_guaranteed()

        # Always allocate WiFi buffers in PSRAM if available
        add_idf_sdkconfig_option("CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP", True)

        if psram_guaranteed:
            _LOGGER.info(
                "Applying high-performance WiFi settings (PSRAM guaranteed): 512 RX buffers, 32 TX buffers"
            )
            # PSRAM is guaranteed - use aggressive settings
            # Higher maximum values are allowed because CONFIG_LWIP_WND_SCALE is set to true in networking component
            # Based on https://github.com/espressif/esp-adf/issues/297#issuecomment-783811702

            # Large dynamic RX buffers (requires PSRAM)
            add_idf_sdkconfig_option("CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM", 16)
            add_idf_sdkconfig_option("CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM", 512)

            # Static TX buffers for better performance
            add_idf_sdkconfig_option("CONFIG_ESP_WIFI_STATIC_TX_BUFFER", True)
            add_idf_sdkconfig_option("CONFIG_ESP_WIFI_TX_BUFFER_TYPE", 0)
            add_idf_sdkconfig_option("CONFIG_ESP_WIFI_CACHE_TX_BUFFER_NUM", 32)
            add_idf_sdkconfig_option("CONFIG_ESP_WIFI_STATIC_TX_BUFFER_NUM", 8)

            # AMPDU settings optimized for PSRAM
            add_idf_sdkconfig_option("CONFIG_ESP_WIFI_AMPDU_TX_ENABLED", True)
            add_idf_sdkconfig_option("CONFIG_ESP_WIFI_TX_BA_WIN", 16)
            add_idf_sdkconfig_option("CONFIG_ESP_WIFI_AMPDU_RX_ENABLED", True)
            add_idf_sdkconfig_option("CONFIG_ESP_WIFI_RX_BA_WIN", 32)
        else:
            _LOGGER.info(
                "Applying optimized WiFi settings: 64 RX buffers, 64 TX buffers"
            )
            # PSRAM not guaranteed - use more conservative, but still optimized settings
            # Based on https://github.com/espressif/esp-idf/blob/release/v5.4/examples/wifi/iperf/sdkconfig.defaults.esp32

            # Standard buffer counts
            add_idf_sdkconfig_option("CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM", 16)
            add_idf_sdkconfig_option("CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM", 64)
            add_idf_sdkconfig_option("CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM", 64)

            # Standard AMPDU settings
            add_idf_sdkconfig_option("CONFIG_ESP_WIFI_AMPDU_TX_ENABLED", True)
            add_idf_sdkconfig_option("CONFIG_ESP_WIFI_TX_BA_WIN", 32)
            add_idf_sdkconfig_option("CONFIG_ESP_WIFI_AMPDU_RX_ENABLED", True)
            add_idf_sdkconfig_option("CONFIG_ESP_WIFI_RX_BA_WIN", 32)

    cg.add_define("USE_WIFI")

    # must register before OTA safe mode check
    await cg.register_component(var, config)

    await cg.past_safe_mode()

    if on_connect_config := config.get(CONF_ON_CONNECT):
        await automation.build_automation(
            var.get_connect_trigger(), [], on_connect_config
        )

    if on_disconnect_config := config.get(CONF_ON_DISCONNECT):
        await automation.build_automation(
            var.get_disconnect_trigger(), [], on_disconnect_config
        )

    CORE.add_job(final_step)


@automation.register_condition("wifi.connected", WiFiConnectedCondition, cv.Schema({}))
async def wifi_connected_to_code(config, condition_id, template_arg, args):
    return cg.new_Pvariable(condition_id, template_arg)


@automation.register_condition("wifi.enabled", WiFiEnabledCondition, cv.Schema({}))
async def wifi_enabled_to_code(config, condition_id, template_arg, args):
    return cg.new_Pvariable(condition_id, template_arg)


@automation.register_condition("wifi.ap_active", WiFiAPActiveCondition, cv.Schema({}))
async def wifi_ap_active_to_code(config, condition_id, template_arg, args):
    return cg.new_Pvariable(condition_id, template_arg)


@automation.register_action("wifi.enable", WiFiEnableAction, cv.Schema({}))
async def wifi_enable_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg)


@automation.register_action("wifi.disable", WiFiDisableAction, cv.Schema({}))
async def wifi_disable_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg)


KEEP_SCAN_RESULTS_KEY = "wifi_keep_scan_results"
RUNTIME_POWER_SAVE_KEY = "wifi_runtime_power_save"
WIFI_LISTENERS_KEY = "wifi_listeners"


def request_wifi_scan_results():
    """Request that WiFi scan results be kept in memory after connection.

    Components that need access to scan results after WiFi is connected should
    call this function during their code generation. This prevents the WiFi component from
    freeing scan result memory after successful connection.
    """
    CORE.data[KEEP_SCAN_RESULTS_KEY] = True


def enable_runtime_power_save_control():
    """Enable runtime WiFi power save control.

    Components that need to dynamically switch WiFi power saving on/off for latency
    performance (e.g., audio streaming, large data transfers) should call this
    function during their code generation. This enables the request_high_performance()
    and release_high_performance() APIs.

    Only supported on ESP32.
    """
    CORE.data[RUNTIME_POWER_SAVE_KEY] = True


def request_wifi_listeners() -> None:
    """Request that WiFi state listeners be compiled in.

    Components that need to be notified about WiFi state changes (IP address changes,
    scan results, connection state) should call this function during their code generation.
    This enables the add_ip_state_listener(), add_scan_results_listener(),
    and add_connect_state_listener() APIs.
    """
    CORE.data[WIFI_LISTENERS_KEY] = True


@coroutine_with_priority(CoroPriority.FINAL)
async def final_step():
    """Final code generation step to configure optional WiFi features."""
    if CORE.data.get(KEEP_SCAN_RESULTS_KEY, False):
        cg.add(
            cg.RawExpression("wifi::global_wifi_component->set_keep_scan_results(true)")
        )
    if CORE.data.get(RUNTIME_POWER_SAVE_KEY, False):
        cg.add_define("USE_WIFI_RUNTIME_POWER_SAVE")
    if CORE.data.get(WIFI_LISTENERS_KEY, False):
        cg.add_define("USE_WIFI_LISTENERS")


@automation.register_action(
    "wifi.configure",
    WiFiConfigureAction,
    cv.Schema(
        {
            cv.Required(CONF_SSID): cv.templatable(cv.ssid),
            cv.Required(CONF_PASSWORD): cv.templatable(validate_password),
            cv.Optional(CONF_SAVE, default=True): cv.templatable(cv.boolean),
            cv.Optional(CONF_TIMEOUT, default="30000ms"): cv.templatable(
                cv.positive_time_period_milliseconds
            ),
            cv.Optional(CONF_ON_CONNECT): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_ERROR): automation.validate_automation(single=True),
        }
    ),
)
async def wifi_set_sta_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    ssid = await cg.templatable(config[CONF_SSID], args, cg.std_string)
    password = await cg.templatable(config[CONF_PASSWORD], args, cg.std_string)
    save = await cg.templatable(config[CONF_SAVE], args, cg.bool_)
    timeout = await cg.templatable(config.get(CONF_TIMEOUT), args, cg.uint32)
    cg.add(var.set_ssid(ssid))
    cg.add(var.set_password(password))
    cg.add(var.set_save(save))
    cg.add(var.set_connection_timeout(timeout))
    if on_connect_config := config.get(CONF_ON_CONNECT):
        await automation.build_automation(
            var.get_connect_trigger(), [], on_connect_config
        )
    if on_error_config := config.get(CONF_ON_ERROR):
        await automation.build_automation(var.get_error_trigger(), [], on_error_config)
    await cg.register_component(var, config)
    return var


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "wifi_component_esp_idf.cpp": {
            PlatformFramework.ESP32_IDF,
            PlatformFramework.ESP32_ARDUINO,
        },
        "wifi_component_esp8266.cpp": {PlatformFramework.ESP8266_ARDUINO},
        "wifi_component_libretiny.cpp": {
            PlatformFramework.BK72XX_ARDUINO,
            PlatformFramework.RTL87XX_ARDUINO,
            PlatformFramework.LN882X_ARDUINO,
        },
        "wifi_component_pico_w.cpp": {PlatformFramework.RP2040_ARDUINO},
    }
)
