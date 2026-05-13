import esphome.codegen as cg
from esphome.components.esp32 import add_idf_component
from esphome.config_helpers import filter_source_files_from_platform, get_logger_level
import esphome.config_validation as cv
from esphome.const import (
    CONF_DISABLED,
    CONF_ID,
    CONF_PORT,
    CONF_PROTOCOL,
    CONF_SERVICE,
    CONF_SERVICES,
    PlatformFramework,
)
from esphome.core import CORE, Lambda, coroutine_with_priority
from esphome.coroutine import CoroPriority
from esphome.cpp_generator import LambdaExpression
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["network"]

# Components that create mDNS services at runtime
# IMPORTANT: If you add a new component here, you must also update the corresponding
# #ifdef blocks in mdns_component.cpp compile_records_() method
COMPONENTS_WITH_MDNS_SERVICES = ("api", "prometheus", "sendspin", "web_server")

mdns_ns = cg.esphome_ns.namespace("mdns")
MDNSComponent = mdns_ns.class_("MDNSComponent", cg.Component)
MDNSTXTRecord = mdns_ns.struct("MDNSTXTRecord")
MDNSService = mdns_ns.struct("MDNSService")


def _remove_id_if_disabled(value):
    value = value.copy()
    if value[CONF_DISABLED]:
        value.pop(CONF_ID)
    return value


CONF_TXT = "txt"

SERVICE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SERVICE): cv.string,
        cv.Required(CONF_PROTOCOL): cv.string,
        cv.Optional(CONF_PORT, default=0): cv.templatable(cv.Any(0, cv.port)),
        cv.Optional(CONF_TXT, default={}): {cv.string: cv.templatable(cv.string)},
    }
)


def _consume_mdns_sockets(config: ConfigType) -> ConfigType:
    """Register socket needs for mDNS component."""
    if config.get(CONF_DISABLED):
        return config

    from esphome.components import socket

    # mDNS needs 2 sockets (IPv4 + IPv6 multicast)
    socket.consume_sockets(2, "mdns", socket.SocketType.UDP)(config)
    return config


def _require_network_interface(config: ConfigType) -> ConfigType:
    """Require a network interface for mDNS on Arduino/LEAmDNS platforms.

    On ESP8266 and RP2040 the C++ implementation needs at least one IP state
    listener (WiFi on ESP8266; WiFi or Ethernet on RP2040) to arm its polling
    window. Reject at config time rather than silently producing a component
    that never initializes.
    """
    if config.get(CONF_DISABLED) or not (CORE.is_esp8266 or CORE.is_rp2040):
        return config
    full_config = fv.full_config.get()
    has_wifi = "wifi" in full_config
    has_ethernet = CORE.is_rp2040 and "ethernet" in full_config
    if not (has_wifi or has_ethernet):
        options = "'wifi'" if CORE.is_esp8266 else "'wifi' or 'ethernet'"
        raise cv.Invalid(
            "mdns on this platform requires a network interface — "
            f"add a {options} component to your configuration."
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MDNSComponent),
            cv.Optional(CONF_DISABLED, default=False): cv.boolean,
            cv.Optional(CONF_SERVICES, default=[]): cv.ensure_list(SERVICE_SCHEMA),
        }
    ),
    _remove_id_if_disabled,
    _consume_mdns_sockets,
)


FINAL_VALIDATE_SCHEMA = _require_network_interface


def mdns_txt_record(key: str, value: str) -> cg.RawExpression:
    """Create a mDNS TXT record.

    Public API for external components. Do not remove.

    Args:
        key: The TXT record key
        value: The TXT record value (static string only)

    Returns:
        A RawExpression representing a MDNSTXTRecord struct
    """
    return cg.RawExpression(
        f"{{MDNS_STR({cg.safe_exp(key)}), MDNS_STR({cg.safe_exp(value)})}}"
    )


async def _mdns_txt_record_templated(
    mdns_comp: cg.Pvariable, key: str, value: Lambda | str
) -> cg.RawExpression:
    """Create a mDNS TXT record with support for templated values.

    Internal helper function.

    Args:
        mdns_comp: The MDNSComponent instance (from cg.get_variable())
        key: The TXT record key
        value: The TXT record value (can be a static string or a lambda template)

    Returns:
        A RawExpression representing a MDNSTXTRecord struct
    """
    if not cg.is_template(value):
        # It's a static string - use directly in flash, no need to store in vector
        return mdns_txt_record(key, value)
    # It's a lambda - evaluate and store using helper
    templated_value = await cg.templatable(value, [], cg.std_string)
    safe_key = cg.safe_exp(key)
    dynamic_call = f"{mdns_comp}->add_dynamic_txt_value(({templated_value})())"
    return cg.RawExpression(f"{{MDNS_STR({safe_key}), MDNS_STR({dynamic_call})}}")


def mdns_service(
    service: str, proto: str, port: int, txt_records: list[cg.RawExpression]
) -> cg.StructInitializer:
    """Create a mDNS service.

    Public API for external components. Do not remove.

    Args:
        service: Service name (e.g., "_http")
        proto: Protocol (e.g., "_tcp" or "_udp")
        port: Port number
        txt_records: List of MDNSTXTRecord expressions

    Returns:
        A StructInitializer representing a MDNSService struct
    """
    # Wrap port in a stateless lambda for TemplatableFn storage.
    # Can't use cg.templatable() here because this is a sync function.
    if not isinstance(port, LambdaExpression):
        port = LambdaExpression(
            f"return {cg.safe_exp(port)};", [], capture="", return_type=cg.uint16
        )
    return cg.StructInitializer(
        MDNSService,
        ("service_type", cg.RawExpression(f"MDNS_STR({cg.safe_exp(service)})")),
        ("proto", cg.RawExpression(f"MDNS_STR({cg.safe_exp(proto)})")),
        ("port", port),
        ("txt_records", txt_records),
    )


def enable_mdns_storage():
    """Enable persistent storage of mDNS services in the MDNSComponent.

    Called by external components (like OpenThread) that need access to
    services after setup() completes via get_services().

    Public API for external components. Do not remove.
    """
    cg.add_define("USE_MDNS_STORE_SERVICES")


@coroutine_with_priority(CoroPriority.NETWORK_SERVICES)
async def to_code(config):
    if config[CONF_DISABLED] is True:
        return

    if CORE.using_arduino:
        if CORE.is_esp8266:
            cg.add_library("ESP8266mDNS", None)
        elif CORE.is_rp2040:
            cg.add_library("LEAmDNS", None)

        # Subscribe to the network IP state listener(s) so MDNS.update() is only
        # scheduled during the probe+announce phase. Same on_ip_state() override
        # serves both WiFi and Ethernet (signatures match).
        if CORE.is_esp8266 or CORE.is_rp2040:
            if "wifi" in CORE.config:
                from esphome.components import wifi

                wifi.request_wifi_ip_state_listener()
            if CORE.is_rp2040 and "ethernet" in CORE.config:
                from esphome.components import ethernet

                ethernet.request_ethernet_ip_state_listener()

    if CORE.is_esp32:
        add_idf_component(name="espressif/mdns", ref="1.11.0")

    cg.add_define("USE_MDNS")

    # Calculate compile-time service count
    service_count = sum(
        1 for key in COMPONENTS_WITH_MDNS_SERVICES if key in CORE.config
    ) + len(config[CONF_SERVICES])

    if config[CONF_SERVICES]:
        cg.add_define("USE_MDNS_EXTRA_SERVICES")
        # Extra services need to be stored persistently
        enable_mdns_storage()

    # Ensure at least 1 service (fallback service)
    cg.add_define("MDNS_SERVICE_COUNT", max(1, service_count))

    # Calculate compile-time dynamic TXT value count
    # Dynamic values are those that cannot be stored in flash at compile time
    # Note: MAC address is now stored in a fixed char[13] buffer, not dynamic storage
    dynamic_txt_count = 0
    # User-provided templatable TXT values (only lambdas, not static strings)
    dynamic_txt_count += sum(
        1
        for service in config[CONF_SERVICES]
        for txt_value in service[CONF_TXT].values()
        if cg.is_template(txt_value)
    )

    # Only add define if we actually need dynamic storage
    if dynamic_txt_count > 0:
        cg.add_define("USE_MDNS_DYNAMIC_TXT")
        cg.add_define("MDNS_DYNAMIC_TXT_COUNT", dynamic_txt_count)

    # Enable storage if verbose logging is enabled (for dump_config)
    if get_logger_level() in ("VERBOSE", "VERY_VERBOSE"):
        enable_mdns_storage()

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    for service in config[CONF_SERVICES]:
        txt_records = [
            await _mdns_txt_record_templated(var, txt_key, txt_value)
            for txt_key, txt_value in service[CONF_TXT].items()
        ]

        exp = mdns_service(
            service[CONF_SERVICE],
            service[CONF_PROTOCOL],
            await cg.templatable(service[CONF_PORT], [], cg.uint16),
            txt_records,
        )

        cg.add(var.add_extra_service(exp))


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "mdns_esp32.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        "mdns_esp8266.cpp": {PlatformFramework.ESP8266_ARDUINO},
        "mdns_host.cpp": {PlatformFramework.HOST_NATIVE},
        "mdns_rp2040.cpp": {PlatformFramework.RP2040_ARDUINO},
        "mdns_libretiny.cpp": {
            PlatformFramework.BK72XX_ARDUINO,
            PlatformFramework.RTL87XX_ARDUINO,
            PlatformFramework.LN882X_ARDUINO,
        },
    }
)
