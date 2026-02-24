import ipaddress
import logging

import esphome.codegen as cg
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.components.psram import is_guaranteed as psram_is_guaranteed
import esphome.config_validation as cv
from esphome.const import CONF_ENABLE_IPV6, CONF_MIN_IPV6_ADDR_COUNT, CONF_PRIORITY
from esphome.core import CORE, CoroPriority, coroutine_with_priority
import esphome.final_validate as fv

CODEOWNERS = ["@esphome/core"]
AUTO_LOAD = ["mdns"]

_LOGGER = logging.getLogger(__name__)

# High performance networking tracking infrastructure
# Components can request high performance networking and this configures lwip and WiFi settings
KEY_HIGH_PERFORMANCE_NETWORKING = "high_performance_networking"
CONF_ENABLE_HIGH_PERFORMANCE = "enable_high_performance"

# Network priority tracking
KEY_NETWORK_PRIORITY = "network_priority"
VALID_NETWORK_TYPES = ["ethernet", "wifi"]
# Setup priority base values — first in list gets the highest priority
NETWORK_PRIORITY_BASE = 300.0
NETWORK_PRIORITY_STEP = 100.0
network_ns = cg.esphome_ns.namespace("network")
IPAddress = network_ns.class_("IPAddress")


def ip_address_literal(ip: str | int | None) -> cg.MockObj:
    """Generate an IPAddress with compile-time initialization instead of runtime parsing.

    This function parses the IP address in Python during code generation and generates
    a call to the 4-octet constructor (IPAddress(192, 168, 1, 1)) instead of the
    string constructor (IPAddress("192.168.1.1")). This eliminates runtime string
    parsing overhead and reduces flash usage on embedded systems.

    Args:
        ip: IP address as string (e.g., "192.168.1.1"), ipaddress.IPv4Address, or None

    Returns:
        IPAddress expression that uses 4-octet constructor for efficiency
    """
    if ip is None:
        return IPAddress(0, 0, 0, 0)

    try:
        # Parse using Python's ipaddress module
        ip_obj = ipaddress.ip_address(ip)
    except (ValueError, TypeError):
        pass
    else:
        # Only support IPv4 for now
        if isinstance(ip_obj, ipaddress.IPv4Address):
            # Extract octets from the packed bytes representation
            octets = ip_obj.packed
            # Generate call to 4-octet constructor: IPAddress(192, 168, 1, 1)
            return IPAddress(octets[0], octets[1], octets[2], octets[3])

    # Fallback to string constructor if parsing fails
    return IPAddress(str(ip))


def require_high_performance_networking() -> None:
    """Request high performance networking for network and WiFi.

    Call this from components that need optimized network performance for streaming
    or high-throughput data transfer. This enables high performance mode which
    configures both lwip TCP settings and WiFi driver settings for improved
    network performance.

    Settings applied (ESP-IDF only):
    - lwip: Larger TCP buffers, windows, and mailbox sizes
    - WiFi: Increased RX/TX buffers, AMPDU aggregation, PSRAM allocation (set by wifi component)

    Configuration is PSRAM-aware:
    - With PSRAM guaranteed: Aggressive settings (512 RX buffers, 512KB TCP windows)
    - Without PSRAM: Conservative optimized settings (64 buffers, 65KB TCP windows)

    Example:
        from esphome.components import network

        def _request_high_performance_networking(config):
            network.require_high_performance_networking()
            return config

        CONFIG_SCHEMA = cv.All(
            ...,
            _request_high_performance_networking,
        )
    """
    # Only set up once (idempotent - multiple components can call this)
    if not CORE.data.get(KEY_HIGH_PERFORMANCE_NETWORKING, False):
        CORE.data[KEY_HIGH_PERFORMANCE_NETWORKING] = True


def has_high_performance_networking() -> bool:
    """Check if high performance networking mode is enabled.

    Returns True when high performance networking has been requested by a
    component or explicitly enabled in the network configuration. This indicates
    that lwip and WiFi will use optimized buffer sizes and settings.

    This function should be called during code generation (to_code phase) by
    components that need to apply performance-related settings.

    Returns:
        bool: True if high performance networking is enabled, False otherwise
    """
    return CORE.data.get(KEY_HIGH_PERFORMANCE_NETWORKING, False)


def get_network_priority(iface: str) -> float | None:
    """Get the setup priority for the given network interface type.

    Returns the float setup priority for ``iface`` based on the order declared
    under ``network: priority:``.  Interfaces listed first receive a higher
    setup priority so they are initialised before lower-priority ones.

    If no ``network: priority:`` has been configured this returns ``None`` and
    the calling component should fall back to its own default setup priority.

    Args:
        iface: Interface type string — one of ``"ethernet"`` or ``"wifi"``
               (case-insensitive).

    Returns:
        float setup priority, or None if no priority list was configured.

    Example usage inside a component's ``to_code``::

        from esphome.components import network

        async def to_code(config):
            var = cg.new_Pvariable(config[CONF_ID])
            await cg.register_component(var, config)

            prio = network.get_network_priority("ethernet")
            if prio is not None:
                cg.add(var.set_setup_priority(prio))
            ...
    """
    priority_list = CORE.data.get(KEY_NETWORK_PRIORITY)
    if priority_list is None:
        return None
    iface_lower = iface.lower()
    try:
        idx = priority_list.index(iface_lower)
    except ValueError:
        return None
    return NETWORK_PRIORITY_BASE - (idx * NETWORK_PRIORITY_STEP)


def _validate_priority_list(value):
    """Ensure the priority list has no duplicates and only valid interface names."""
    value = cv.ensure_list(cv.one_of(*VALID_NETWORK_TYPES, lower=True))(value)
    if len(value) != len(set(value)):
        raise cv.Invalid("Duplicate entries are not allowed in 'priority'")
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.SplitDefault(
            CONF_ENABLE_IPV6,
            esp8266=False,
            esp32=False,
            rp2040=False,
            bk72xx=False,
            host=False,
        ): cv.All(
            cv.boolean,
            cv.Any(
                cv.require_framework_version(
                    esp_idf=cv.Version(0, 0, 0),
                    esp32_arduino=cv.Version(0, 0, 0),
                    esp8266_arduino=cv.Version(0, 0, 0),
                    rp2040_arduino=cv.Version(0, 0, 0),
                    bk72xx_arduino=cv.Version(1, 7, 0),
                    host=cv.Version(0, 0, 0),
                ),
                cv.boolean_false,
            ),
        ),
        cv.Optional(CONF_MIN_IPV6_ADDR_COUNT, default=0): cv.positive_int,
        cv.Optional(CONF_ENABLE_HIGH_PERFORMANCE): cv.All(cv.boolean, cv.only_on_esp32),
        cv.Optional(CONF_PRIORITY): _validate_priority_list,
    }
)


def _final_validate(config):
    """Check that every interface named in 'priority' has a corresponding component block."""
    full = fv.full_config.get()
    for iface in config.get(CONF_PRIORITY, []):
        if iface not in full:
            raise cv.Invalid(
                f"'{iface}' is listed in 'network: priority:' but no '{iface}:' "
                f"component is configured",
                [CONF_PRIORITY],
            )


FINAL_VALIDATE_SCHEMA = _final_validate


@coroutine_with_priority(CoroPriority.NETWORK)
async def to_code(config):
    cg.add_define("USE_NETWORK")
    # ESP32 with Arduino uses ESP-IDF network APIs directly, no Arduino Network library needed

    # Store the user-declared network priority list in CORE.data so that the
    # ethernet and wifi components can query it via get_network_priority() during
    # their own to_code phase.
    if CONF_PRIORITY in config:
        priority_list = config[CONF_PRIORITY]
        CORE.data[KEY_NETWORK_PRIORITY] = priority_list
        _LOGGER.info("Network interface priority: %s", " > ".join(priority_list))

    # Apply high performance networking settings
    # Config can explicitly enable/disable, or default to component-driven behavior
    enable_high_perf = config.get(CONF_ENABLE_HIGH_PERFORMANCE)
    component_requested = CORE.data.get(KEY_HIGH_PERFORMANCE_NETWORKING, False)

    # Explicit config overrides component request
    should_enable = (
        enable_high_perf if enable_high_perf is not None else component_requested
    )

    # Log when user explicitly disables but a component requested it
    if enable_high_perf is False and component_requested:
        _LOGGER.info(
            "High performance networking disabled by user configuration (overriding component request)"
        )

    if CORE.is_esp32 and should_enable:
        # Check if PSRAM is guaranteed (set by psram component during final validation)
        psram_guaranteed = psram_is_guaranteed()

        if psram_guaranteed:
            _LOGGER.info(
                "Applying high-performance lwip settings (PSRAM guaranteed): 512KB TCP windows, 512 mailbox sizes"
            )
            # PSRAM is guaranteed - use aggressive settings
            # Higher maximum values are allowed because CONFIG_LWIP_WND_SCALE is set to true
            # CONFIG_LWIP_WND_SCALE can only be enabled if CONFIG_SPIRAM_IGNORE_NOTFOUND isn't set
            # Based on https://github.com/espressif/esp-adf/issues/297#issuecomment-783811702

            # Enable window scaling for much larger TCP windows
            add_idf_sdkconfig_option("CONFIG_LWIP_WND_SCALE", True)
            add_idf_sdkconfig_option("CONFIG_LWIP_TCP_RCV_SCALE", 3)

            # Large TCP buffers and windows (requires PSRAM)
            add_idf_sdkconfig_option("CONFIG_LWIP_TCP_SND_BUF_DEFAULT", 65534)
            add_idf_sdkconfig_option("CONFIG_LWIP_TCP_WND_DEFAULT", 512000)

            # Large mailboxes for high throughput
            add_idf_sdkconfig_option("CONFIG_LWIP_TCPIP_RECVMBOX_SIZE", 512)
            add_idf_sdkconfig_option("CONFIG_LWIP_TCP_RECVMBOX_SIZE", 512)

            # TCP connection limits
            add_idf_sdkconfig_option("CONFIG_LWIP_MAX_ACTIVE_TCP", 16)
            add_idf_sdkconfig_option("CONFIG_LWIP_MAX_LISTENING_TCP", 16)

            # TCP optimizations
            add_idf_sdkconfig_option("CONFIG_LWIP_TCP_MAXRTX", 12)
            add_idf_sdkconfig_option("CONFIG_LWIP_TCP_SYNMAXRTX", 6)
            add_idf_sdkconfig_option("CONFIG_LWIP_TCP_MSS", 1436)
            add_idf_sdkconfig_option("CONFIG_LWIP_TCP_MSL", 60000)
            add_idf_sdkconfig_option("CONFIG_LWIP_TCP_OVERSIZE_MSS", True)
            add_idf_sdkconfig_option("CONFIG_LWIP_TCP_QUEUE_OOSEQ", True)
        else:
            _LOGGER.info(
                "Applying optimized lwip settings: 65KB TCP windows, 64 mailbox sizes"
            )
            # PSRAM not guaranteed - use more conservative, but still optimized settings
            # Based on https://github.com/espressif/esp-idf/blob/release/v5.4/examples/wifi/iperf/sdkconfig.defaults.esp32
            add_idf_sdkconfig_option("CONFIG_LWIP_TCP_SND_BUF_DEFAULT", 65534)
            add_idf_sdkconfig_option("CONFIG_LWIP_TCP_WND_DEFAULT", 65534)
            add_idf_sdkconfig_option("CONFIG_LWIP_TCP_RECVMBOX_SIZE", 64)
            add_idf_sdkconfig_option("CONFIG_LWIP_TCPIP_RECVMBOX_SIZE", 64)

    if (enable_ipv6 := config.get(CONF_ENABLE_IPV6, None)) is not None:
        cg.add_define("USE_NETWORK_IPV6", enable_ipv6)
        if enable_ipv6:
            cg.add_define(
                "USE_NETWORK_MIN_IPV6_ADDR_COUNT", config[CONF_MIN_IPV6_ADDR_COUNT]
            )
        if CORE.is_esp32:
            if CORE.using_arduino:
                add_idf_sdkconfig_option("CONFIG_LWIP_IPV6", True)
                add_idf_sdkconfig_option("CONFIG_LWIP_IPV6_AUTOCONFIG", True)
            else:
                add_idf_sdkconfig_option("CONFIG_LWIP_IPV6", enable_ipv6)
                add_idf_sdkconfig_option("CONFIG_LWIP_IPV6_AUTOCONFIG", enable_ipv6)
        elif enable_ipv6:
            cg.add_build_flag("-DCONFIG_LWIP_IPV6")
            cg.add_build_flag("-DCONFIG_LWIP_IPV6_AUTOCONFIG")
            if CORE.is_rp2040:
                cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_ENABLE_IPV6")
            if CORE.is_esp8266:
                cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_LWIP2_IPV6_LOW_MEMORY")
            if CORE.is_bk72xx:
                cg.add_build_flag("-DCONFIG_IPV6")
