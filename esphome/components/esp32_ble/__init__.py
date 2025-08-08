from enum import Enum
import re

from esphome import automation
import esphome.codegen as cg
from esphome.components.esp32 import add_idf_sdkconfig_option, const, get_esp32_variant
import esphome.config_validation as cv
from esphome.const import CONF_ENABLE_ON_BOOT, CONF_ESPHOME, CONF_ID, CONF_NAME
from esphome.core import CORE, TimePeriod
from esphome.core.config import CONF_NAME_ADD_MAC_SUFFIX
import esphome.final_validate as fv

DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@jesserockz", "@Rapsssito", "@bdraco"]
DOMAIN = "esp32_ble"


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


# Set to track which loggers are needed by components
_required_loggers: set[BTLoggers] = set()


def register_bt_logger(*loggers: BTLoggers) -> None:
    """Register Bluetooth logger categories that a component needs.

    Args:
        *loggers: One or more BTLoggers enum members
    """
    for logger in loggers:
        if not isinstance(logger, BTLoggers):
            raise TypeError(
                f"Logger must be a BTLoggers enum member, got {type(logger)}"
            )
        _required_loggers.add(logger)


CONF_BLE_ID = "ble_id"
CONF_IO_CAPABILITY = "io_capability"
CONF_ADVERTISING = "advertising"
CONF_ADVERTISING_CYCLE_TIME = "advertising_cycle_time"
CONF_DISABLE_BT_LOGS = "disable_bt_logs"
CONF_CONNECTION_TIMEOUT = "connection_timeout"
CONF_MAX_NOTIFICATIONS = "max_notifications"

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
        cv.SplitDefault(CONF_DISABLE_BT_LOGS, esp32_idf=True): cv.All(
            cv.only_with_esp_idf, cv.boolean
        ),
        cv.SplitDefault(CONF_CONNECTION_TIMEOUT, esp32_idf="20s"): cv.All(
            cv.only_with_esp_idf,
            cv.positive_time_period_seconds,
            cv.Range(min=TimePeriod(seconds=10), max=TimePeriod(seconds=180)),
        ),
        cv.SplitDefault(CONF_MAX_NOTIFICATIONS, esp32_idf=12): cv.All(
            cv.only_with_esp_idf,
            cv.positive_int,
            cv.Range(min=1, max=64),
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

    return config


FINAL_VALIDATE_SCHEMA = final_validation


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_enable_on_boot(config[CONF_ENABLE_ON_BOOT]))
    cg.add(var.set_io_capability(config[CONF_IO_CAPABILITY]))
    cg.add(var.set_advertising_cycle_time(config[CONF_ADVERTISING_CYCLE_TIME]))
    if (name := config.get(CONF_NAME)) is not None:
        cg.add(var.set_name(name))
    await cg.register_component(var, config)

    if CORE.using_esp_idf:
        add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)
        add_idf_sdkconfig_option("CONFIG_BT_BLE_42_FEATURES_SUPPORTED", True)

        # Register the core BLE loggers that are always needed
        register_bt_logger(BTLoggers.GAP, BTLoggers.BTM, BTLoggers.HCI)

        # Apply logger settings if log disabling is enabled
        if config.get(CONF_DISABLE_BT_LOGS, False):
            # Disable all Bluetooth loggers that are not required
            for logger in BTLoggers:
                if logger not in _required_loggers:
                    add_idf_sdkconfig_option(f"{logger.value}_NONE", True)

        # Set BLE connection establishment timeout to match aioesphomeapi/bleak-retry-connector
        # Default is 20 seconds instead of ESP-IDF's 30 seconds. Because there is no way to
        # cancel a BLE connection in progress, when aioesphomeapi times out at 20 seconds,
        # the connection slot remains occupied for the remaining time, preventing new connection
        # attempts and wasting valuable connection slots.
        if CONF_CONNECTION_TIMEOUT in config:
            timeout_seconds = int(config[CONF_CONNECTION_TIMEOUT].total_seconds)
            add_idf_sdkconfig_option(
                "CONFIG_BT_BLE_ESTAB_LINK_CONN_TOUT", timeout_seconds
            )

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


@automation.register_condition("ble.enabled", BLEEnabledCondition, cv.Schema({}))
async def ble_enabled_to_code(config, condition_id, template_arg, args):
    return cg.new_Pvariable(condition_id, template_arg)


@automation.register_action("ble.enable", BLEEnableAction, cv.Schema({}))
async def ble_enable_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg)


@automation.register_action("ble.disable", BLEDisableAction, cv.Schema({}))
async def ble_disable_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg)
