import ipaddress
import logging
from typing import Any

import esphome.codegen as cg
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.components.psram import is_guaranteed as psram_is_guaranteed
from esphome.components.zephyr import zephyr_add_prj_conf
import esphome.config_validation as cv
from esphome.const import (
    CONF_ENABLE_IPV6,
    CONF_ID,
    CONF_MIN_IPV6_ADDR_COUNT,
    CONF_PRIORITY,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]
AUTO_LOAD = ["mdns"]

_LOGGER = logging.getLogger(__name__)

# High performance networking tracking infrastructure
# Components can request high performance networking and this configures lwip and WiFi settings
KEY_HIGH_PERFORMANCE_NETWORKING = "high_performance_networking"
CONF_ENABLE_HIGH_PERFORMANCE = "enable_high_performance"

# Network priority tracking infrastructure
# Components can query this to determine their relative setup priority.
# CORE.data[KEY_NETWORK_PRIORITY] is a list of dicts of the form
# {"interface": "ethernet"}, in user-declared order.
KEY_NETWORK_PRIORITY = "network_priority"

# Only interfaces whose component already calls get_network_priority() are
# accepted in the priority list. openthread and modem will be added here when
# they wire up their setup-priority consumer in their own to_code — see
# NETWORK_PLAN.md for the full multi-interface roadmap.
VALID_NETWORK_TYPES = ["ethernet", "wifi"]

# Interfaces NetworkComponent::loop() knows how to arbitrate the default route
# for. Deliberately NOT derived from VALID_NETWORK_TYPES: extending that list
# without extending the C++ arbitration (and then this set) is caught in
# _final_validate() as a config error instead of a silently mis-routed interface.
ARBITRATED_NETWORK_TYPES = frozenset({"ethernet", "wifi"})

# Setup priority base values — first in list gets the highest priority.
#
# The base equals the historical setup_priority::WIFI / ::ETHERNET default
# (250.0), so a single-entry priority list yields exactly the same setup order
# as a config with no priority block. Subsequent entries step down by a small
# amount to break ties without crossing other priority bands.
#
# Important: must stay strictly less than setup_priority::AFTER_BLUETOOTH
# (300.0), which NetworkComponent itself uses — otherwise the highest-priority
# interface could tie with NetworkComponent and run before esp_netif_init().
NETWORK_PRIORITY_BASE = 250.0
NETWORK_PRIORITY_STEP = 5.0

# Lower-bound guard. The lowest-priority entry gets
# NETWORK_PRIORITY_BASE - (len - 1) * NETWORK_PRIORITY_STEP, which must stay
# strictly above setup_priority::AFTER_WIFI (200.0, see esphome/core/component.h)
# so a long priority list never drops an interface into the band used by
# components that expect to run after the network is up. There is ample headroom
# for the two types today; this check raises if a future expansion of
# VALID_NETWORK_TYPES would silently cross that band. Uses an explicit raise
# rather than a bare assert so the guard isn't stripped under python -O/-OO.
_SETUP_PRIORITY_AFTER_WIFI = 200.0
if (
    NETWORK_PRIORITY_BASE - (len(VALID_NETWORK_TYPES) - 1) * NETWORK_PRIORITY_STEP
    <= _SETUP_PRIORITY_AFTER_WIFI
):
    raise ValueError(
        "network: priority: list is long enough to cross setup_priority::AFTER_WIFI"
    )

network_ns = cg.esphome_ns.namespace("network")
NetworkComponent = network_ns.class_("NetworkComponent", cg.Component)
IPAddress = network_ns.class_("IPAddress")


def _register_provisioning_source(config: ConfigType) -> ConfigType:
    """Register network connectivity as a provisioning source.

    The network component is auto-loaded whenever an interface (wifi, ethernet, ...)
    is configured, so a device with connectivity always has this source: it is
    considered provisioned once it has connected via any interface, and
    `provisioning:` is valid without another source.
    """
    from esphome.components import provisioning

    provisioning.register_source("network")
    return config


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


def add_use_address(var: cg.MockObj, use_address: str) -> None:
    """Generate a set_use_address() call only when the address must be baked in.

    The default "<name>.local" is not stored in the firmware; it is rebuilt at
    runtime from the device name (see network::get_use_address_to()), which also
    picks up the MAC suffix when name_add_mac_suffix is enabled. A compile-time
    string could never include that suffix, so baking it in would log the wrong
    address.
    """
    if use_address != f"{CORE.name}.local":
        cg.add(var.set_use_address(use_address))


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


def validate_ipv6(value: bool) -> bool:
    if CORE.is_nrf52 and not value:
        raise cv.Invalid("On nRF52, enable_ipv6 must be true")

    return value


def get_network_priority(iface: str) -> float | None:
    """Get the setup priority for the given network interface type.

    Returns the float setup priority for ``iface`` based on the order declared
    under ``network: priority:``.  Interfaces listed first receive a higher
    setup priority so they are initialised before lower-priority ones.

    If no ``network: priority:`` has been configured this returns ``None`` and
    the calling component should fall back to its own default setup priority.

    Args:
        iface: Interface type string (case-insensitive). Currently ``"ethernet"``
               or ``"wifi"`` — the only types the priority-list validator
               accepts; ``"openthread"`` / ``"modem"`` are planned but not yet
               supported. An interface not present in the configured list
               returns ``None``.

    Returns:
        float setup priority, or None if no priority list was configured.

    Example usage inside a component's ``to_code``. Emit the override before
    ``register_component`` so an explicit ``setup_priority:`` on the component
    still wins::

        from esphome.components import network

        async def to_code(config):
            var = cg.new_Pvariable(config[CONF_ID])

            prio = network.get_network_priority("ethernet")
            if prio is not None:
                cg.set_setup_priority(var, prio)

            await cg.register_component(var, config)
            ...
    """
    priority_list = CORE.data.get(KEY_NETWORK_PRIORITY)
    if priority_list is None:
        return None
    iface_lower = iface.lower()
    for idx, entry in enumerate(priority_list):
        if entry["interface"] == iface_lower:
            return NETWORK_PRIORITY_BASE - (idx * NETWORK_PRIORITY_STEP)
    return None


def get_priority_interfaces_from_full_config(full_config: ConfigType) -> set[str]:
    """Return the set of interface names declared in ``network: priority:``.

    Reads from the full validated config (``fv.full_config.get()``) and is
    intended for use inside ``FINAL_VALIDATE_SCHEMA`` hooks, before
    ``to_code`` has run and ``CORE.data`` has been populated.  Returns an
    empty set if no priority list was configured.
    """
    return {
        entry["interface"]
        for entry in full_config.get("network", {}).get(CONF_PRIORITY, [])
    }


def _validate_priority_list(value: Any) -> list[dict[str, str]]:
    """Validate and normalize the priority list, rejecting duplicates.

    Each entry is the name of one network interface (one of
    ``VALID_NETWORK_TYPES``). Mixed-case input is accepted and normalized
    to lowercase.  The normalized list is a list of dicts of the form
    ``{"interface": "ethernet"}`` so that future per-entry options can be
    added without breaking call sites.
    """
    raw = cv.ensure_list(cv.one_of(*VALID_NETWORK_TYPES, lower=True))(value)
    entries = [{"interface": iface} for iface in raw]
    interfaces = [e["interface"] for e in entries]
    if len(interfaces) != len(set(interfaces)):
        raise cv.Invalid("Duplicate entries are not allowed in 'priority'")
    return entries


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(NetworkComponent),
            cv.SplitDefault(
                CONF_ENABLE_IPV6,
                bk72xx=False,
                esp32=False,
                esp8266=False,
                host=False,
                rp2=False,
                nrf52=True,
            ): cv.All(
                cv.boolean,
                cv.Any(
                    cv.require_framework_version(
                        bk72xx_arduino=cv.Version(1, 7, 0),
                        esp_idf=cv.Version(0, 0, 0),
                        esp32_arduino=cv.Version(0, 0, 0),
                        esp8266_arduino=cv.Version(0, 0, 0),
                        host=cv.Version(0, 0, 0),
                        rp2_arduino=cv.Version(0, 0, 0),
                        nrf52_zephyr=cv.Version(0, 0, 0),
                    ),
                    cv.boolean_false,
                ),
                validate_ipv6,
            ),
            cv.Optional(CONF_MIN_IPV6_ADDR_COUNT, default=0): cv.positive_int,
            cv.Optional(CONF_ENABLE_HIGH_PERFORMANCE): cv.All(
                cv.boolean, cv.only_on_esp32
            ),
            cv.Optional(CONF_PRIORITY): _validate_priority_list,
        }
    ),
    _register_provisioning_source,
)


def _final_validate(config: ConfigType) -> None:
    """Check that every interface named in 'priority' has a corresponding component block."""
    full = fv.full_config.get()
    priority_list = config.get(CONF_PRIORITY, [])
    for entry in priority_list:
        iface = entry["interface"]
        if iface not in full:
            raise cv.Invalid(
                f"'{iface}' is listed in 'network: priority:' but no '{iface}:' "
                f"component is configured",
                [CONF_PRIORITY],
            )

    # Tripwire for future interface types (openthread, modem): the C++ default-route
    # arbitration pivots on USE_NETWORK_PRIMARY_INTERFACE_WIFI and only knows
    # ethernet and wifi. Extend NetworkComponent::loop() before allowing another
    # type here. Unreachable until VALID_NETWORK_TYPES grows.
    if (
        len(priority_list) > 1
        and (
            unsupported := {e["interface"] for e in priority_list}
            - ARBITRATED_NETWORK_TYPES
        )
        and CORE.is_esp32
    ):
        raise cv.Invalid(
            "Default-route arbitration does not support: "
            f"{', '.join(sorted(unsupported))}",
            [CONF_PRIORITY],
        )


FINAL_VALIDATE_SCHEMA = _final_validate


@coroutine_with_priority(CoroPriority.NETWORK)
async def to_code(config):
    cg.add_define("USE_NETWORK")
    # ESP32 with Arduino uses ESP-IDF network APIs directly, no Arduino Network library needed

    # Store the user-declared network priority list in CORE.data so that ethernet,
    # wifi and other network components can query it via get_network_priority()
    # during their own to_code phase.
    if CONF_PRIORITY in config:
        priority_list = config[CONF_PRIORITY]
        CORE.data[KEY_NETWORK_PRIORITY] = priority_list
        # network/util.cpp resolves the reported address (get_use_address_to,
        # get_ip_addresses) in a fixed ethernet-first order; a wifi-first priority
        # list is the only case that deviates from it, so it is the only case that
        # needs a define.
        if priority_list[0]["interface"] == "wifi":
            cg.add_define("USE_NETWORK_PRIMARY_INTERFACE_WIFI")

        # With more than one interface, NetworkComponent::loop() arbitrates the
        # default route (ESP-IDF's fixed route_prio values would always favor
        # WiFi). ESP32 only: the arbitration needs esp_netif, which both
        # frameworks build from source.
        # The ethernet/wifi-only assumption behind the arbitration is enforced in
        # _final_validate() so a future unsupported type fails as a config error.
        if len(priority_list) > 1 and CORE.is_esp32:
            cg.add_define("USE_NETWORK_DEFAULT_ROUTE")
            # Have lwIP switch to the DNS servers of the netif that owns the
            # default route whenever the arbitration changes it.
            add_idf_sdkconfig_option("CONFIG_ESP_NETIF_SET_DNS_PER_DEFAULT_NETIF", True)

        _LOGGER.info(
            "Network interface priority: %s",
            " > ".join(entry["interface"] for entry in priority_list),
        )

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

    if CORE.is_nrf52:
        zephyr_add_prj_conf("NETWORKING", True)
        zephyr_add_prj_conf("NET_IPV6", True)
        zephyr_add_prj_conf("NET_TCP", True)
        zephyr_add_prj_conf("NET_UDP", True)
        # The nRF Connect SDK replaces mbedTLS with PSA/Oberon crypto and does not provide the
        # legacy mbedtls_md5() symbol that Zephyr's RFC 6528 TCP ISN generator links against
        # (selecting MBEDTLS_MAC_MD5_ENABLED does not bring in the legacy C API here). Disable it so
        # TCP links; Zephyr falls back to sys_rand32_get() for the ISN (randomized, but not the
        # RFC 6528 keyed hash).
        zephyr_add_prj_conf("NET_TCP_ISN_RFC6528", False)
        # Enlarge the Zephyr network buffer pool and TCP windows for the Thread path.
        # Zephyr's defaults are tiny: NET_BUF_TX_COUNT=16 * NET_BUF_DATA_SIZE=128 is only
        # ~2 KB of TX data -- barely one 1280-byte IPv6 packet once 6LoWPAN fragments it.
        # The ESPHome API entity-sync burst overruns that instantly, so socket writes fail
        # with ENOBUFS ("Buffer full") and the connection is dropped. ESP32 sidesteps this
        # by enlarging the lwIP TCP window (CONFIG_LWIP_TCP_* above); give Zephyr the
        # equivalent headroom, sized to RAM and the Thread 1280-byte MTU (not ESP32's 64 KB).
        # The bounded send window also provides flow control so TCP stops queueing past
        # what the buffer pool can hold instead of erroring.
        zephyr_add_prj_conf("NET_PKT_RX_COUNT", 24)
        zephyr_add_prj_conf("NET_PKT_TX_COUNT", 24)
        zephyr_add_prj_conf("NET_BUF_RX_COUNT", 48)
        zephyr_add_prj_conf("NET_BUF_TX_COUNT", 48)
        zephyr_add_prj_conf("NET_TCP_MAX_RECV_WINDOW_SIZE", 2280)
        zephyr_add_prj_conf("NET_TCP_MAX_SEND_WINDOW_SIZE", 2280)

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
            if CORE.is_bk72xx:
                cg.add_build_flag("-DCONFIG_IPV6")
            if CORE.is_esp8266:
                cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_LWIP2_IPV6_LOW_MEMORY")
            if CORE.is_rp2:
                cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_ENABLE_IPV6")
    # Pvariable creation lives in a separate coroutine at NETWORK_SERVICES so it
    # emits after wifi/ethernet at COMMUNICATION. This keeps compile-time config
    # (above) separate from C++ object lifecycle and allows wiring in interface
    # pointers via get_variable().
    if CORE.is_esp32:
        CORE.add_job(network_component_to_code, config)


@coroutine_with_priority(CoroPriority.NETWORK_SERVICES)
async def network_component_to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
