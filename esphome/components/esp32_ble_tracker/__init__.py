from __future__ import annotations

import copy
from dataclasses import dataclass
import logging

from esphome import automation
import esphome.codegen as cg
from esphome.components import ble_device_base, esp32_ble, ota
from esphome.components.ble_device_base import CONF_CONNECTION_SCAN_WINDOW
from esphome.components.const import CONF_ON_SCAN_END, CONF_SCAN_PARAMETERS, CONF_WINDOW
from esphome.components.esp32 import (
    add_idf_sdkconfig_option,
    idf_version,
    request_bluetooth,
    request_software_coexistence,
)
from esphome.components.esp32_ble import (
    IDF_MAX_CONNECTIONS,
    BTLoggers,
    bt_uuid,
    bt_uuid16_format,
    bt_uuid32_format,
    bt_uuid128_format,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTIVE,
    CONF_CONTINUOUS,
    CONF_DURATION,
    CONF_ID,
    CONF_INTERVAL,
    CONF_MAC_ADDRESS,
    CONF_MANUFACTURER_ID,
    CONF_MAX_CONNECTIONS,
    CONF_ON_BLE_ADVERTISE,
    CONF_ON_BLE_MANUFACTURER_DATA_ADVERTISE,
    CONF_ON_BLE_SERVICE_DATA_ADVERTISE,
    CONF_SERVICE_UUID,
    CONF_TRIGGER_ID,
)
from esphome.core import CORE, ID, CoroPriority, TimePeriod, coroutine_with_priority
from esphome.cpp_generator import MockObj, TemplateArgsType
from esphome.enum import StrEnum
from esphome.types import ConfigType

DOMAIN = "esp32_ble_tracker"

AUTO_LOAD = ["ble_device_base", "esp32_ble"]
DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@bdraco"]

ble_device_base.register_hub_provider("esp32_ble_tracker")

CONF_ESP32_BLE_ID = "esp32_ble_id"
CONF_SOFTWARE_COEXISTENCE = "software_coexistence"

_LOGGER = logging.getLogger(__name__)


# Enum for BLE features
class BLEFeatures(StrEnum):
    ESP_BT_DEVICE = "ESP_BT_DEVICE"


# CORE.data key for state management
ESP32_BLE_TRACKER_REQUIRED_FEATURES_KEY = "esp32_ble_tracker_required_features"


def _get_required_features() -> set[BLEFeatures]:
    """Get the set of required BLE features from CORE.data."""
    return CORE.data.setdefault(ESP32_BLE_TRACKER_REQUIRED_FEATURES_KEY, set())


# Slot counters sizing the tracker's StaticVector storage; one request per
# registered listener or client.
CLIENT_COUNT_DEFINE = "ESPHOME_ESP32_BLE_TRACKER_CLIENT_COUNT"
_request_listener_slot = cg.slot_counter("ESPHOME_ESP32_BLE_TRACKER_LISTENER_COUNT")
_request_client_slot = cg.slot_counter(CLIENT_COUNT_DEFINE)


def register_ble_features(features: set[BLEFeatures]) -> None:
    """Register BLE features that a component needs.

    Args:
        features: Set of BLEFeatures enum members
    """
    _get_required_features().update(features)


esp32_ble_tracker_ns = cg.esphome_ns.namespace("esp32_ble_tracker")
ESP32BLETracker = esp32_ble_tracker_ns.class_(
    "ESP32BLETracker",
    ble_device_base.BLEHub,
    cg.Component,
    cg.Parented.template(esp32_ble.ESP32BLE),
)
ESPBTClient = esp32_ble_tracker_ns.class_("ESPBTClient")
ESPBTDeviceListener = esp32_ble_tracker_ns.class_("ESPBTDeviceListener")
ESPBTDevice = esp32_ble_tracker_ns.class_("ESPBTDevice")
ESPBTDeviceConstRef = ESPBTDevice.operator("ref").operator("const")
adv_data_t = cg.std_vector.template(cg.uint8)
adv_data_t_const_ref = adv_data_t.operator("ref").operator("const")
# Triggers
ESPBTAdvertiseTrigger = esp32_ble_tracker_ns.class_(
    "ESPBTAdvertiseTrigger", automation.Trigger.template(ESPBTDeviceConstRef)
)
BLEServiceDataAdvertiseTrigger = esp32_ble_tracker_ns.class_(
    "BLEServiceDataAdvertiseTrigger", automation.Trigger.template(adv_data_t_const_ref)
)
BLEManufacturerDataAdvertiseTrigger = esp32_ble_tracker_ns.class_(
    "BLEManufacturerDataAdvertiseTrigger",
    automation.Trigger.template(adv_data_t_const_ref),
)
BLEEndOfScanTrigger = esp32_ble_tracker_ns.class_(
    "BLEEndOfScanTrigger", automation.Trigger.template()
)
# Actions
ESP32BLEStartScanAction = esp32_ble_tracker_ns.class_(
    "ESP32BLEStartScanAction", automation.Action
)
ESP32BLEStopScanAction = esp32_ble_tracker_ns.class_(
    "ESP32BLEStopScanAction", automation.Action
)


def validate_max_connections_deprecated(config: ConfigType) -> ConfigType:
    if CONF_MAX_CONNECTIONS in config:
        _LOGGER.warning(
            "The 'max_connections' option in 'esp32_ble_tracker' is deprecated. "
            "Please move it to the 'esp32_ble' component instead."
        )
    return config


# ESP-IDF 5.5.5 fixed a coexistence bug on the ESP32 where BLE scans ran far
# longer than the configured window (espressif/esp-idf#18931). Before the fix,
# the default 30 ms window in a 320 ms interval effectively scanned at a much
# higher duty cycle than requested; with the fix, that same default only
# listens 9.4 % of the time and misses most advertisements when wifi shares
# the radio. Espressif recommends setting the window equal to the interval in
# that case: the coexistence arbiter still shares the radio with wifi, and
# BLE uses the airtime wifi does not claim.
IDF_SCAN_WINDOW_FIX_VERSION = cv.Version(5, 5, 5)

# Above this the scanner holds the shared radio long enough that wifi drops
# packets and connections on some access points (others cope fine, which is
# why this is a warning and not an error); old proxy configs with 1100 ms
# windows are a recurring cause of instability (esphome/esphome#18655). Only
# wifi shares the radio; long windows are fine on ethernet builds.
MAX_RECOMMENDED_WIFI_SCAN_WINDOW = TimePeriod(milliseconds=600)


@dataclass
class TrackerData:
    """Per-run validation state, namespaced under DOMAIN in CORE.data."""

    scan_window_defaulted: bool = False
    connection_window_injected: bool = False


def _get_data() -> TrackerData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = TrackerData()
    return CORE.data[DOMAIN]


def _scan_window_default() -> TimePeriod:
    """Schema default for the scan window.

    Records that the user did not set a window, so _raise_defaulted_scan_window
    can tell a defaulted 30 ms from an explicit one; the raise itself must wait
    for the outer schema because it depends on software_coexistence, a sibling
    key not yet resolved here.
    """
    _get_data().scan_window_defaulted = True
    return cv.positive_time_period(ble_device_base.DEFAULT_SCAN_WINDOW)


def _raise_defaulted_scan_window(config: ConfigType) -> ConfigType:
    """Raise a defaulted scan window to the interval where that is safe.

    Only when the coexistence arbiter is compiled in (software_coexistence,
    present iff wifi is configured and not disabled by the user) and the IDF
    honors the window strictly (>= 5.5.5); without the arbiter a full-duty
    scan would starve wifi outright, and a user-set window is never touched.
    Raising to the interval cannot invalidate the already-validated
    parameters, so no re-validation is needed. The connection window is
    checked against the window here, after the raise.
    """
    params = config[CONF_SCAN_PARAMETERS]
    if (
        _get_data().scan_window_defaulted
        and config.get(CONF_SOFTWARE_COEXISTENCE)
        and idf_version() >= IDF_SCAN_WINDOW_FIX_VERSION
    ):
        # Copy so the config dump shows a plain value instead of a YAML
        # anchor/alias pair pointing at the interval.
        params[CONF_WINDOW] = copy.copy(params[CONF_INTERVAL])
        # Arm the connection-time fallback unless the user set one. Injected
        # after validation; safe because it equals the validated window default.
        if CONF_CONNECTION_SCAN_WINDOW not in params:
            params[CONF_CONNECTION_SCAN_WINDOW] = cv.positive_time_period(
                ble_device_base.DEFAULT_SCAN_WINDOW
            )
            _get_data().connection_window_injected = True
    if (
        connection_window := params.get(CONF_CONNECTION_SCAN_WINDOW)
    ) is not None and connection_window > params[CONF_WINDOW]:
        # A larger value would widen the scan during connections.
        raise cv.Invalid(
            f"{CONF_CONNECTION_SCAN_WINDOW} ({connection_window}) needs to be "
            f"smaller than the scan window ({params[CONF_WINDOW]})",
            path=[CONF_SCAN_PARAMETERS, CONF_CONNECTION_SCAN_WINDOW],
        )
    return config


def _warn_long_scan_window_with_wifi(config: ConfigType) -> ConfigType:
    """Warn when the scan window is long enough to starve wifi.

    Runs after _raise_defaulted_scan_window so it sees the final window.
    software_coexistence is only present when wifi is configured, so ethernet
    builds never warn: BLE has the radio to itself there. Presence is what
    matters, not the value; with the arbiter disabled a long window starves
    wifi outright.
    """
    params = config[CONF_SCAN_PARAMETERS]
    window = params[CONF_WINDOW]
    if CONF_SOFTWARE_COEXISTENCE not in config:
        return config
    if window <= MAX_RECOMMENDED_WIFI_SCAN_WINDOW:
        return config
    if _get_data().scan_window_defaulted:
        # The window was raised to match the interval, so point at the key the
        # user actually set.
        _LOGGER.warning(
            "BLE scan interval of %s sets the scan window to the same value, "
            "which starves wifi on the same radio and can cause wifi disconnects "
            "depending on the access point; keep the interval at or below %s "
            "(for example interval: 320ms). Long windows are only a problem with "
            "wifi, they are fine on ethernet",
            params[CONF_INTERVAL],
            MAX_RECOMMENDED_WIFI_SCAN_WINDOW,
        )
        return config
    _LOGGER.warning(
        "BLE scan window of %s with wifi on the same radio starves wifi and "
        "can cause wifi disconnects depending on the access point; keep the "
        "window at or below %s (for example interval: 320ms, window: 300ms). "
        "Long windows are only a problem with wifi, they are fine on ethernet",
        window,
        MAX_RECOMMENDED_WIFI_SCAN_WINDOW,
    )
    return config


# 320 ms is the ESP-IDF reference scan interval; the shared schema also
# tightens validation to the controller's 2.5 ms .. 10240 ms range and rejects
# window/interval pairs that collapse to the same 0.625 ms unit count.
# The window default is conditional (see _scan_window_default above).
SCAN_PARAMETERS_SCHEMA = ble_device_base.scan_parameters_schema(
    "320ms", window_default=_scan_window_default, connection_window=True
)

# Codegen helpers are owned by ble_device_base; kept under the historical names
# here for the components that import them from this module.
as_hex = ble_device_base.as_hex
as_hex_array = ble_device_base.as_hex_array
as_reversed_hex_array = ble_device_base.as_reversed_hex_array


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ESP32BLETracker),
            cv.GenerateID(esp32_ble.CONF_BLE_ID): cv.use_id(esp32_ble.ESP32BLE),
            cv.Optional(CONF_MAX_CONNECTIONS): cv.All(
                cv.positive_int, cv.Range(min=0, max=IDF_MAX_CONNECTIONS)
            ),
            cv.Optional(CONF_SCAN_PARAMETERS, default={}): SCAN_PARAMETERS_SCHEMA,
            cv.Optional(CONF_ON_BLE_ADVERTISE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        ESPBTAdvertiseTrigger
                    ),
                    cv.Optional(CONF_MAC_ADDRESS): cv.ensure_list(cv.mac_address),
                }
            ),
            cv.Optional(
                CONF_ON_BLE_SERVICE_DATA_ADVERTISE
            ): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        BLEServiceDataAdvertiseTrigger
                    ),
                    cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
                    cv.Required(CONF_SERVICE_UUID): bt_uuid,
                }
            ),
            cv.Optional(
                CONF_ON_BLE_MANUFACTURER_DATA_ADVERTISE
            ): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        BLEManufacturerDataAdvertiseTrigger
                    ),
                    cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
                    cv.Required(CONF_MANUFACTURER_ID): bt_uuid,
                }
            ),
            cv.Optional(CONF_ON_SCAN_END): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BLEEndOfScanTrigger)}
            ),
            cv.OnlyWith(CONF_SOFTWARE_COEXISTENCE, "wifi", default=True): bool,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_max_connections_deprecated,
    _raise_defaulted_scan_window,
    _warn_long_scan_window_with_wifi,
)


FINAL_VALIDATE_SCHEMA = esp32_ble.validate_variant

ESP_BLE_DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ESP32_BLE_ID): cv.use_id(ESP32BLETracker),
    }
)


async def to_code(config: ConfigType) -> None:
    # Register the loggers this component needs
    esp32_ble.register_bt_logger(BTLoggers.BLE_SCAN)

    # Behavior parity with the pre-split tracker: IRK resolution is always
    # available on esp32 (sensors with irk: worked without opting in).
    ble_device_base.request_irk_support()

    # Selects the BLEHub alias arm in ble_device_base/ble_hub_impl.h.
    cg.add_define("USE_ESP32_BLE_TRACKER")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[esp32_ble.CONF_BLE_ID])
    esp32_ble.register_gap_event_handler(parent, var)
    esp32_ble.register_gap_scan_event_handler(parent, var)
    esp32_ble.register_gattc_event_handler(parent, var)
    esp32_ble.register_ble_status_event_handler(parent, var)
    cg.add(var.set_parent(parent))

    params = config[CONF_SCAN_PARAMETERS]
    cg.add(var.set_scan_duration(params[CONF_DURATION]))
    cg.add(var.set_scan_interval(ble_device_base.to_ble_units(params[CONF_INTERVAL])))
    cg.add(var.set_scan_window(ble_device_base.to_ble_units(params[CONF_WINDOW])))
    if (connection_window := params.get(CONF_CONNECTION_SCAN_WINDOW)) is not None:
        # Emitted at FINAL so a scan-only build, where the guarded C++ path
        # compiles out, skips the call entirely.
        window_units = ble_device_base.to_ble_units(connection_window)

        @coroutine_with_priority(CoroPriority.FINAL)
        async def _emit_connection_scan_window() -> None:
            if cg.get_slot_count(CLIENT_COUNT_DEFINE):
                cg.add(var.set_connection_scan_window(window_units))
            elif not _get_data().connection_window_injected:
                # Warn only for a user-set value; the injected default drops silently.
                _LOGGER.warning(
                    "'%s' has no effect because this build has no BLE client "
                    "components (for example bluetooth_proxy with active "
                    "connections, or ble_client)",
                    CONF_CONNECTION_SCAN_WINDOW,
                )

        CORE.add_job(_emit_connection_scan_window)
    cg.add(var.set_scan_active(params[CONF_ACTIVE]))
    cg.add(var.set_scan_continuous(params[CONF_CONTINUOUS]))

    # Register ESP_BT_DEVICE feature if any of the automation triggers are used
    if (
        config.get(CONF_ON_BLE_ADVERTISE)
        or config.get(CONF_ON_BLE_SERVICE_DATA_ADVERTISE)
        or config.get(CONF_ON_BLE_MANUFACTURER_DATA_ADVERTISE)
    ):
        register_ble_features({BLEFeatures.ESP_BT_DEVICE})

    for conf in config.get(CONF_ON_BLE_ADVERTISE, []):
        _request_listener_slot()
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if CONF_MAC_ADDRESS in conf:
            addr_list = [it.as_hex for it in conf[CONF_MAC_ADDRESS]]
            cg.add(trigger.set_addresses(addr_list))
        await automation.build_automation(trigger, [(ESPBTDeviceConstRef, "x")], conf)
    for conf in config.get(CONF_ON_BLE_SERVICE_DATA_ADVERTISE, []):
        _request_listener_slot()
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if len(conf[CONF_SERVICE_UUID]) == len(bt_uuid16_format):
            cg.add(trigger.set_service_uuid16(as_hex(conf[CONF_SERVICE_UUID])))
        elif len(conf[CONF_SERVICE_UUID]) == len(bt_uuid32_format):
            cg.add(trigger.set_service_uuid32(as_hex(conf[CONF_SERVICE_UUID])))
        elif len(conf[CONF_SERVICE_UUID]) == len(bt_uuid128_format):
            uuid128 = as_reversed_hex_array(conf[CONF_SERVICE_UUID])
            cg.add(trigger.set_service_uuid128(uuid128))
        if CONF_MAC_ADDRESS in conf:
            cg.add(trigger.set_address(conf[CONF_MAC_ADDRESS].as_hex))
        await automation.build_automation(trigger, [(adv_data_t_const_ref, "x")], conf)
    for conf in config.get(CONF_ON_BLE_MANUFACTURER_DATA_ADVERTISE, []):
        _request_listener_slot()
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if len(conf[CONF_MANUFACTURER_ID]) == len(bt_uuid16_format):
            cg.add(trigger.set_manufacturer_uuid16(as_hex(conf[CONF_MANUFACTURER_ID])))
        elif len(conf[CONF_MANUFACTURER_ID]) == len(bt_uuid32_format):
            cg.add(trigger.set_manufacturer_uuid32(as_hex(conf[CONF_MANUFACTURER_ID])))
        elif len(conf[CONF_MANUFACTURER_ID]) == len(bt_uuid128_format):
            uuid128 = as_reversed_hex_array(conf[CONF_MANUFACTURER_ID])
            cg.add(trigger.set_manufacturer_uuid128(uuid128))
        if CONF_MAC_ADDRESS in conf:
            cg.add(trigger.set_address(conf[CONF_MAC_ADDRESS].as_hex))
        await automation.build_automation(trigger, [(adv_data_t_const_ref, "x")], conf)
    for conf in config.get(CONF_ON_SCAN_END, []):
        _request_listener_slot()
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    request_bluetooth()
    if config.get(CONF_SOFTWARE_COEXISTENCE):
        request_software_coexistence()
    # https://github.com/espressif/esp-idf/issues/4101
    # https://github.com/espressif/esp-idf/issues/2503
    # Match arduino CONFIG_BTU_TASK_STACK_SIZE
    # https://github.com/espressif/arduino-esp32/blob/fd72cf46ad6fc1a6de99c1d83ba8eba17d80a4ee/tools/sdk/esp32/sdkconfig#L1866
    add_idf_sdkconfig_option("CONFIG_BT_BTU_TASK_STACK_SIZE", 8192)
    # Note: CONFIG_BT_ACL_CONNECTIONS and CONFIG_BTDM_CTRL_BLE_MAX_CONN are now
    # configured in esp32_ble component based on max_connections setting

    ota.request_ota_state_listeners()  # To be notified when an OTA update starts
    cg.add_define("USE_ESP32_BLE_CLIENT")

    CORE.add_job(_add_ble_features)

    if config.get(CONF_SOFTWARE_COEXISTENCE):
        cg.add_define("USE_ESP32_BLE_SOFTWARE_COEXISTENCE")


# This needs to be run as a job with very low priority so that all components have
# chance to call register_ble_tracker and register_client before the list is checked
# and added to the global defines list.
@coroutine_with_priority(CoroPriority.FINAL)
async def _add_ble_features() -> None:
    # Add feature-specific defines based on what's needed
    required_features = _get_required_features()
    # Sensors registered through the neutral ble_device_base path (BLEHub) need
    # the parsed-device pipeline compiled in, exactly like esp32-path listeners.
    if cg.get_slot_count(ble_device_base.LISTENER_COUNT_DEFINE):
        # The neutral (BLEHub) listener count define itself is emitted by
        # ble_device_base's own job; only the feature coupling lives here.
        required_features.add(BLEFeatures.ESP_BT_DEVICE)
    if BLEFeatures.ESP_BT_DEVICE in required_features:
        cg.add_define("USE_ESP32_BLE_DEVICE")
        cg.add_define("USE_ESP32_BLE_UUID")


ESP32_BLE_START_SCAN_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(ESP32BLETracker),
        cv.Optional(CONF_CONTINUOUS, default=False): cv.templatable(cv.boolean),
    }
)


@automation.register_action(
    "esp32_ble_tracker.start_scan",
    ESP32BLEStartScanAction,
    ESP32_BLE_START_SCAN_ACTION_SCHEMA,
    synchronous=True,
)
async def esp32_ble_tracker_start_scan_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_CONTINUOUS], args, cg.bool_)
    cg.add(var.set_continuous(template_))
    return var


ESP32_BLE_STOP_SCAN_ACTION_SCHEMA = automation.maybe_simple_id(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(ESP32BLETracker),
        }
    )
)


@automation.register_action(
    "esp32_ble_tracker.stop_scan",
    ESP32BLEStopScanAction,
    ESP32_BLE_STOP_SCAN_ACTION_SCHEMA,
    synchronous=True,
)
async def esp32_ble_tracker_stop_scan_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


async def register_ble_device(
    var: cg.SafeExpType, config: ConfigType
) -> cg.SafeExpType:
    register_ble_features({BLEFeatures.ESP_BT_DEVICE})
    _request_listener_slot()
    paren = await cg.get_variable(config[CONF_ESP32_BLE_ID])
    cg.add(paren.register_listener(var))
    return var


async def register_client(var: cg.SafeExpType, config: ConfigType) -> cg.SafeExpType:
    register_ble_features({BLEFeatures.ESP_BT_DEVICE})
    _request_client_slot()
    paren = await cg.get_variable(config[CONF_ESP32_BLE_ID])
    cg.add(paren.register_client(var))
    return var


async def register_raw_client(
    var: cg.SafeExpType, config: ConfigType
) -> cg.SafeExpType:
    """Register a BLE client that only needs raw advertisement data.

    This does NOT register the ESP_BT_DEVICE feature, meaning ESPBTDevice
    will not be compiled in if this is the only registration method used.
    """
    _request_client_slot()
    paren = await cg.get_variable(config[CONF_ESP32_BLE_ID])
    cg.add(paren.register_client(var))
    return var
