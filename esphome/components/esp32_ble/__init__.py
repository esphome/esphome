from collections.abc import Callable, MutableMapping
from dataclasses import dataclass
from enum import Enum
import logging
import re
from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components import socket
from esphome.components.esp32 import add_idf_sdkconfig_option, const, get_esp32_variant
import esphome.config_validation as cv
from esphome.const import (
    CONF_ENABLE_ON_BOOT,
    CONF_ESPHOME,
    CONF_ID,
    CONF_MAX_CONNECTIONS,
    CONF_NAME,
    CONF_NAME_ADD_MAC_SUFFIX,
)
from esphome.core import CORE, CoroPriority, TimePeriod, coroutine_with_priority
import esphome.final_validate as fv

DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["socket"]
CODEOWNERS = ["@jesserockz", "@Rapsssito", "@bdraco"]
DOMAIN = "esp32_ble"

_LOGGER = logging.getLogger(__name__)


class BTLoggers(Enum):
    """Bluetooth logger categories available in ESP-IDF.

    Each logger controls debug output for a specific Bluetooth subsystem.
    The value is the ESP-IDF sdkconfig option name for controlling the log level.
    """

    # Core Stack Layers
    HCI = "CONFIG_BT_LOG_HCI_TRACE_LEVEL"
    """Host Controller Interface - Low-level interface between host and controller"""

    BTM = "CONFIG_BT_LOG_BTM_TRACE_LEVEL"
    """Bluetooth Manager - Core device control, connections, and security"""

    L2CAP = "CONFIG_BT_LOG_L2CAP_TRACE_LEVEL"
    """Logical Link Control and Adaptation Protocol - Connection multiplexing"""

    RFCOMM = "CONFIG_BT_LOG_RFCOMM_TRACE_LEVEL"
    """Serial port emulation over Bluetooth (Classic only)"""

    SDP = "CONFIG_BT_LOG_SDP_TRACE_LEVEL"
    """Service Discovery Protocol - Service discovery (Classic only)"""

    GAP = "CONFIG_BT_LOG_GAP_TRACE_LEVEL"
    """Generic Access Profile - Device discovery and connections"""

    # Network Protocols
    BNEP = "CONFIG_BT_LOG_BNEP_TRACE_LEVEL"
    """Bluetooth Network Encapsulation Protocol - IP over Bluetooth"""

    PAN = "CONFIG_BT_LOG_PAN_TRACE_LEVEL"
    """Personal Area Networking - Ethernet over Bluetooth"""

    # Audio/Video Profiles (Classic Bluetooth)
    A2D = "CONFIG_BT_LOG_A2D_TRACE_LEVEL"
    """Advanced Audio Distribution - A2DP audio streaming"""

    AVDT = "CONFIG_BT_LOG_AVDT_TRACE_LEVEL"
    """Audio/Video Distribution Transport - A2DP transport protocol"""

    AVCT = "CONFIG_BT_LOG_AVCT_TRACE_LEVEL"
    """Audio/Video Control Transport - AVRCP transport protocol"""

    AVRC = "CONFIG_BT_LOG_AVRC_TRACE_LEVEL"
    """Audio/Video Remote Control - Media playback control"""

    # Security
    SMP = "CONFIG_BT_LOG_SMP_TRACE_LEVEL"
    """Security Manager Protocol - BLE pairing and encryption"""

    # Application Layer
    BTIF = "CONFIG_BT_LOG_BTIF_TRACE_LEVEL"
    """Bluetooth Interface - Application interface layer"""

    BTC = "CONFIG_BT_LOG_BTC_TRACE_LEVEL"
    """Bluetooth Common - Task handling and coordination"""

    # BLE Specific
    BLE_SCAN = "CONFIG_BT_LOG_BLE_SCAN_TRACE_LEVEL"
    """BLE scanning operations"""

    GATT = "CONFIG_BT_LOG_GATT_TRACE_LEVEL"
    """Generic Attribute Profile - BLE data exchange protocol"""

    # Other Profiles
    MCA = "CONFIG_BT_LOG_MCA_TRACE_LEVEL"
    """Multi-Channel Adaptation - Health device profile"""

    HID = "CONFIG_BT_LOG_HID_TRACE_LEVEL"
    """Human Interface Device - Keyboards, mice, controllers"""

    APPL = "CONFIG_BT_LOG_APPL_TRACE_LEVEL"
    """Application layer logging"""

    OSI = "CONFIG_BT_LOG_OSI_TRACE_LEVEL"
    """OS abstraction layer - Threading, memory, timers"""

    BLUFI = "CONFIG_BT_LOG_BLUFI_TRACE_LEVEL"
    """ESP32 WiFi provisioning over Bluetooth"""


# Key for storing required loggers in CORE.data
ESP32_BLE_REQUIRED_LOGGERS_KEY = "esp32_ble_required_loggers"


def _get_required_loggers() -> set[BTLoggers]:
    """Get the set of required Bluetooth loggers from CORE.data."""
    return CORE.data.setdefault(ESP32_BLE_REQUIRED_LOGGERS_KEY, set())


# Dataclass for handler registration counts
@dataclass
class HandlerCounts:
    gap_event: int = 0
    gap_scan_event: int = 0
    gattc_event: int = 0
    gatts_event: int = 0
    ble_status_event: int = 0


# Track handler registration counts for StaticVector sizing
_handler_counts = HandlerCounts()


def register_gap_event_handler(parent_var: cg.MockObj, handler_var: cg.MockObj) -> None:
    """Register a GAP event handler and track the count."""
    _handler_counts.gap_event += 1
    cg.add(parent_var.register_gap_event_handler(handler_var))


def register_gap_scan_event_handler(
    parent_var: cg.MockObj, handler_var: cg.MockObj
) -> None:
    """Register a GAP scan event handler and track the count."""
    _handler_counts.gap_scan_event += 1
    cg.add(parent_var.register_gap_scan_event_handler(handler_var))


def register_gattc_event_handler(
    parent_var: cg.MockObj, handler_var: cg.MockObj
) -> None:
    """Register a GATTc event handler and track the count."""
    _handler_counts.gattc_event += 1
    cg.add(parent_var.register_gattc_event_handler(handler_var))


def register_gatts_event_handler(
    parent_var: cg.MockObj, handler_var: cg.MockObj
) -> None:
    """Register a GATTs event handler and track the count."""
    _handler_counts.gatts_event += 1
    cg.add(parent_var.register_gatts_event_handler(handler_var))


def register_ble_status_event_handler(
    parent_var: cg.MockObj, handler_var: cg.MockObj
) -> None:
    """Register a BLE status event handler and track the count."""
    _handler_counts.ble_status_event += 1
    cg.add(parent_var.register_ble_status_event_handler(handler_var))


def register_bt_logger(*loggers: BTLoggers) -> None:
    """Register Bluetooth logger categories that a component needs.

    Args:
        *loggers: One or more BTLoggers enum members
    """
    required_loggers = _get_required_loggers()
    for logger in loggers:
        if not isinstance(logger, BTLoggers):
            raise TypeError(
                f"Logger must be a BTLoggers enum member, got {type(logger)}"
            )
        required_loggers.add(logger)


CONF_BLE_ID = "ble_id"
CONF_IO_CAPABILITY = "io_capability"
CONF_ADVERTISING = "advertising"
CONF_ADVERTISING_CYCLE_TIME = "advertising_cycle_time"
CONF_DISABLE_BT_LOGS = "disable_bt_logs"
CONF_CONNECTION_TIMEOUT = "connection_timeout"
CONF_MAX_NOTIFICATIONS = "max_notifications"

# BLE connection limits
# ESP-IDF CONFIG_BT_ACL_CONNECTIONS has range 1-9, default 4
# Total instances: 10 (ADV + SCAN + connections)
# - ADV only: up to 9 connections
# - SCAN only: up to 9 connections
# - ADV + SCAN: up to 8 connections
DEFAULT_MAX_CONNECTIONS = 3
IDF_MAX_CONNECTIONS = 9

# Connection slot tracking keys
KEY_ESP32_BLE = "esp32_ble"
KEY_USED_CONNECTION_SLOTS = "used_connection_slots"

# Export for use by other components (bluetooth_proxy, etc.)
__all__ = [
    "DEFAULT_MAX_CONNECTIONS",
    "IDF_MAX_CONNECTIONS",
    "KEY_ESP32_BLE",
    "KEY_USED_CONNECTION_SLOTS",
    "consume_connection_slots",
]

NO_BLUETOOTH_VARIANTS = [const.VARIANT_ESP32S2]

esp32_ble_ns = cg.esphome_ns.namespace("esp32_ble")
ESP32BLE = esp32_ble_ns.class_("ESP32BLE", cg.Component)

GAPEventHandler = esp32_ble_ns.class_("GAPEventHandler")
GATTcEventHandler = esp32_ble_ns.class_("GATTcEventHandler")
GATTsEventHandler = esp32_ble_ns.class_("GATTsEventHandler")

BLEEnabledCondition = esp32_ble_ns.class_("BLEEnabledCondition", automation.Condition)
BLEEnableAction = esp32_ble_ns.class_("BLEEnableAction", automation.Action)
BLEDisableAction = esp32_ble_ns.class_("BLEDisableAction", automation.Action)

IoCapability = esp32_ble_ns.enum("IoCapability")
IO_CAPABILITY = {
    "none": IoCapability.IO_CAP_NONE,
    "keyboard_only": IoCapability.IO_CAP_IN,
    "keyboard_display": IoCapability.IO_CAP_KBDISP,
    "display_only": IoCapability.IO_CAP_OUT,
    "display_yes_no": IoCapability.IO_CAP_IO,
}

esp_power_level_t = cg.global_ns.enum("esp_power_level_t")

TX_POWER_LEVELS = {
    -12: esp_power_level_t.ESP_PWR_LVL_N12,
    -9: esp_power_level_t.ESP_PWR_LVL_N9,
    -6: esp_power_level_t.ESP_PWR_LVL_N6,
    -3: esp_power_level_t.ESP_PWR_LVL_N3,
    0: esp_power_level_t.ESP_PWR_LVL_N0,
    3: esp_power_level_t.ESP_PWR_LVL_P3,
    6: esp_power_level_t.ESP_PWR_LVL_P6,
    9: esp_power_level_t.ESP_PWR_LVL_P9,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESP32BLE),
        cv.Optional(CONF_NAME): cv.All(cv.string, cv.Length(max=20)),
        cv.Optional(CONF_IO_CAPABILITY, default="none"): cv.enum(
            IO_CAPABILITY, lower=True
        ),
        cv.Optional(CONF_ENABLE_ON_BOOT, default=True): cv.boolean,
        cv.Optional(CONF_ADVERTISING, default=False): cv.boolean,
        cv.Optional(
            CONF_ADVERTISING_CYCLE_TIME, default="10s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_DISABLE_BT_LOGS, default=True): cv.boolean,
        cv.Optional(CONF_CONNECTION_TIMEOUT, default="20s"): cv.All(
            cv.positive_time_period_seconds,
            cv.Range(min=TimePeriod(seconds=10), max=TimePeriod(seconds=180)),
        ),
        cv.Optional(CONF_MAX_NOTIFICATIONS, default=12): cv.All(
            cv.positive_int,
            cv.Range(min=1, max=64),
        ),
        cv.Optional(CONF_MAX_CONNECTIONS, default=DEFAULT_MAX_CONNECTIONS): cv.All(
            cv.positive_int, cv.Range(min=1, max=IDF_MAX_CONNECTIONS)
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


bt_uuid16_format = "XXXX"
bt_uuid32_format = "XXXXXXXX"
bt_uuid128_format = "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"


def bt_uuid(value):
    in_value = cv.string_strict(value)
    value = in_value.upper()

    if len(value) == len(bt_uuid16_format):
        pattern = re.compile("^[A-F|0-9]{4,}$")
        if not pattern.match(value):
            raise cv.Invalid(
                f"Invalid hexadecimal value for 16 bit UUID format: '{in_value}'"
            )
        return value
    if len(value) == len(bt_uuid32_format):
        pattern = re.compile("^[A-F|0-9]{8,}$")
        if not pattern.match(value):
            raise cv.Invalid(
                f"Invalid hexadecimal value for 32 bit UUID format: '{in_value}'"
            )
        return value
    if len(value) == len(bt_uuid128_format):
        pattern = re.compile(
            "^[A-F|0-9]{8,}-[A-F|0-9]{4,}-[A-F|0-9]{4,}-[A-F|0-9]{4,}-[A-F|0-9]{12,}$"
        )
        if not pattern.match(value):
            raise cv.Invalid(
                f"Invalid hexadecimal value for 128 UUID format: '{in_value}'"
            )
        return value
    raise cv.Invalid(
        f"Bluetooth UUID must be in 16 bit '{bt_uuid16_format}', 32 bit '{bt_uuid32_format}', or 128 bit '{bt_uuid128_format}' format"
    )


def validate_variant(_):
    variant = get_esp32_variant()
    if variant in NO_BLUETOOTH_VARIANTS:
        raise cv.Invalid(f"{variant} does not support Bluetooth")


def consume_connection_slots(
    value: int, consumer: str
) -> Callable[[MutableMapping], MutableMapping]:
    """Reserve BLE connection slots for a component.

    Args:
        value: Number of connection slots to reserve
        consumer: Name of the component consuming the slots

    Returns:
        A validator function that records the slot usage
    """

    def _consume_connection_slots(config: MutableMapping) -> MutableMapping:
        data: dict[str, Any] = CORE.data.setdefault(KEY_ESP32_BLE, {})
        slots: list[str] = data.setdefault(KEY_USED_CONNECTION_SLOTS, [])
        slots.extend([consumer] * value)
        return config

    return _consume_connection_slots


def validate_connection_slots(max_connections: int) -> None:
    """Validate that BLE connection slots don't exceed the configured maximum."""
    # Skip validation in testing mode to allow component grouping
    if CORE.testing_mode:
        return

    ble_data = CORE.data.get(KEY_ESP32_BLE, {})
    used_slots = ble_data.get(KEY_USED_CONNECTION_SLOTS, [])
    num_used = len(used_slots)

    if num_used <= max_connections:
        return

    slot_users = ", ".join(used_slots)

    if num_used > IDF_MAX_CONNECTIONS:
        raise cv.Invalid(
            f"BLE components require {num_used} connection slots but maximum is {IDF_MAX_CONNECTIONS}. "
            f"Reduce the number of BLE clients. Components: {slot_users}"
        )

    _LOGGER.warning(
        "BLE components require %d connection slot(s) but only %d configured. "
        "Please set 'max_connections: %d' in the 'esp32_ble' component. "
        "Components: %s",
        num_used,
        max_connections,
        num_used,
        slot_users,
    )


def final_validation(config):
    validate_variant(config)
    if (name := config.get(CONF_NAME)) is not None:
        full_config = fv.full_config.get()
        max_length = 20
        if full_config[CONF_ESPHOME][CONF_NAME_ADD_MAC_SUFFIX]:
            max_length -= 7  # "-AABBCC" is appended when add mac suffix option is used
        if len(name) > max_length:
            raise cv.Invalid(
                f"Name '{name}' is too long, maximum length is {max_length} characters"
            )

    # Set GATT Client/Server sdkconfig options based on which components are loaded
    full_config = fv.full_config.get()

    # Validate connection slots usage
    max_connections = config.get(CONF_MAX_CONNECTIONS, DEFAULT_MAX_CONNECTIONS)
    validate_connection_slots(max_connections)

    # Check if hosted bluetooth is being used
    if "esp32_hosted" in full_config:
        add_idf_sdkconfig_option("CONFIG_BT_CLASSIC_ENABLED", False)
        add_idf_sdkconfig_option("CONFIG_BT_BLE_ENABLED", True)
        add_idf_sdkconfig_option("CONFIG_BT_BLUEDROID_ENABLED", True)
        add_idf_sdkconfig_option("CONFIG_BT_CONTROLLER_DISABLED", True)
        add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID", True)
        add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_BLUEDROID_HCI_VHCI", True)

    # Check if BLE Server is needed
    has_ble_server = "esp32_ble_server" in full_config

    # Check if BLE Client is needed (via esp32_ble_tracker or esp32_ble_client)
    has_ble_client = (
        "esp32_ble_tracker" in full_config or "esp32_ble_client" in full_config
    )

    # ESP-IDF BLE stack requires GATT Server to be enabled when GATT Client is enabled
    # This is an internal dependency in the Bluedroid stack (tested ESP-IDF 5.4.2-5.5.1)
    # See: https://github.com/espressif/esp-idf/issues/17724
    add_idf_sdkconfig_option("CONFIG_BT_GATTS_ENABLE", has_ble_server or has_ble_client)
    add_idf_sdkconfig_option("CONFIG_BT_GATTC_ENABLE", has_ble_client)

    # Handle max_connections: check for deprecated location in esp32_ble_tracker
    max_connections = config.get(CONF_MAX_CONNECTIONS, DEFAULT_MAX_CONNECTIONS)

    # Use value from tracker if esp32_ble doesn't have it explicitly set (backward compat)
    if "esp32_ble_tracker" in full_config:
        tracker_config = full_config["esp32_ble_tracker"]
        if "max_connections" in tracker_config and CONF_MAX_CONNECTIONS not in config:
            max_connections = tracker_config["max_connections"]

    # Set CONFIG_BT_ACL_CONNECTIONS to the maximum connections needed + 1 for ADV/SCAN
    # This is the Bluedroid host stack total instance limit (range 1-9, default 4)
    # Total instances = ADV/SCAN (1) + connection slots (max_connections)
    # Shared between client (tracker/ble_client) and server
    add_idf_sdkconfig_option("CONFIG_BT_ACL_CONNECTIONS", max_connections + 1)

    # Set controller-specific max connections for ESP32 (classic)
    # CONFIG_BTDM_CTRL_BLE_MAX_CONN is ESP32-specific controller limit (just connections, not ADV/SCAN)
    # For newer chips (C3/S3/etc), different configs are used automatically
    add_idf_sdkconfig_option("CONFIG_BTDM_CTRL_BLE_MAX_CONN", max_connections)

    return config


FINAL_VALIDATE_SCHEMA = final_validation


# This needs to be run as a job with CoroPriority.FINAL priority so that all components have
# a chance to register their handlers before the counts are added to defines.
@coroutine_with_priority(CoroPriority.FINAL)
async def _add_ble_handler_defines():
    # Add defines for StaticVector sizing based on handler registration counts
    # Only define if count > 0 to avoid allocating unnecessary memory
    if _handler_counts.gap_event > 0:
        cg.add_define(
            "ESPHOME_ESP32_BLE_GAP_EVENT_HANDLER_COUNT", _handler_counts.gap_event
        )
    if _handler_counts.gap_scan_event > 0:
        cg.add_define(
            "ESPHOME_ESP32_BLE_GAP_SCAN_EVENT_HANDLER_COUNT",
            _handler_counts.gap_scan_event,
        )
    if _handler_counts.gattc_event > 0:
        cg.add_define(
            "ESPHOME_ESP32_BLE_GATTC_EVENT_HANDLER_COUNT", _handler_counts.gattc_event
        )
    if _handler_counts.gatts_event > 0:
        cg.add_define(
            "ESPHOME_ESP32_BLE_GATTS_EVENT_HANDLER_COUNT", _handler_counts.gatts_event
        )
    if _handler_counts.ble_status_event > 0:
        cg.add_define(
            "ESPHOME_ESP32_BLE_BLE_STATUS_EVENT_HANDLER_COUNT",
            _handler_counts.ble_status_event,
        )


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_enable_on_boot(config[CONF_ENABLE_ON_BOOT]))
    cg.add(var.set_io_capability(config[CONF_IO_CAPABILITY]))
    cg.add(var.set_advertising_cycle_time(config[CONF_ADVERTISING_CYCLE_TIME]))
    if (name := config.get(CONF_NAME)) is not None:
        cg.add(var.set_name(name))
    await cg.register_component(var, config)

    # BLE uses the socket wake_loop_threadsafe() mechanism to wake the main loop from BLE tasks
    # This enables low-latency (~12μs) BLE event processing instead of waiting for
    # select() timeout (0-16ms). The wake socket is shared across all components.
    socket.require_wake_loop_threadsafe()

    # Define max connections for use in C++ code (e.g., ble_server.h)
    max_connections = config.get(CONF_MAX_CONNECTIONS, DEFAULT_MAX_CONNECTIONS)
    cg.add_define("USE_ESP32_BLE_MAX_CONNECTIONS", max_connections)

    add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BT_BLE_42_FEATURES_SUPPORTED", True)

    # Register the core BLE loggers that are always needed
    register_bt_logger(BTLoggers.GAP, BTLoggers.BTM, BTLoggers.HCI)

    # Apply logger settings if log disabling is enabled
    if config.get(CONF_DISABLE_BT_LOGS, False):
        # Disable all Bluetooth loggers that are not required
        required_loggers = _get_required_loggers()
        for logger in BTLoggers:
            if logger not in required_loggers:
                add_idf_sdkconfig_option(f"{logger.value}_NONE", True)

    # Set BLE connection establishment timeout to match aioesphomeapi/bleak-retry-connector
    # Default is 20 seconds instead of ESP-IDF's 30 seconds. Because there is no way to
    # cancel a BLE connection in progress, when aioesphomeapi times out at 20 seconds,
    # the connection slot remains occupied for the remaining time, preventing new connection
    # attempts and wasting valuable connection slots.
    if CONF_CONNECTION_TIMEOUT in config:
        timeout_seconds = int(config[CONF_CONNECTION_TIMEOUT].total_seconds)
        add_idf_sdkconfig_option("CONFIG_BT_BLE_ESTAB_LINK_CONN_TOUT", timeout_seconds)
        # Increase GATT client connection retry count for problematic devices
        # Default in ESP-IDF is 3, we increase to 10 for better reliability with
        # low-power/timing-sensitive devices
        add_idf_sdkconfig_option("CONFIG_BT_GATTC_CONNECT_RETRY_COUNT", 10)

    # Set the maximum number of notification registrations
    # This controls how many BLE characteristics can have notifications enabled
    # across all connections for a single GATT client interface
    # https://github.com/esphome/issues/issues/6808
    if CONF_MAX_NOTIFICATIONS in config:
        add_idf_sdkconfig_option(
            "CONFIG_BT_GATTC_NOTIF_REG_MAX", config[CONF_MAX_NOTIFICATIONS]
        )

    cg.add_define("USE_ESP32_BLE")

    if config[CONF_ADVERTISING]:
        cg.add_define("USE_ESP32_BLE_ADVERTISING")
        cg.add_define("USE_ESP32_BLE_UUID")

    # Schedule the handler defines to be added after all components register
    CORE.add_job(_add_ble_handler_defines)


@automation.register_condition("ble.enabled", BLEEnabledCondition, cv.Schema({}))
async def ble_enabled_to_code(config, condition_id, template_arg, args):
    return cg.new_Pvariable(condition_id, template_arg)


@automation.register_action("ble.enable", BLEEnableAction, cv.Schema({}))
async def ble_enable_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg)


@automation.register_action("ble.disable", BLEDisableAction, cv.Schema({}))
async def ble_disable_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg)
