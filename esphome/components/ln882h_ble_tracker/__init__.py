"""LN882H BLE Tracker — ESPHome BLE 5.1 scanner for the LibreTiny LN882H
(lightning-ln882h family).

Builds on the ln882h_ble controller component (stack bring-up, BLE address,
scan primitives) and implements the platform-neutral ble_device_base BLEHub
contract: the shared BLE sensors (ble_presence, ble_rssi, ble_scanner,
bthome_mithermometer, xiaomi_*, …) bind to this tracker through
cv.use_id(BLEHub) with no LN-specific code.

Scan modes:
  continuous: true  — scan runs forever; never stops automatically.
                      Use this when the radio is dedicated to BLE.
  continuous: false — a started scan runs for `duration` ms, then stops. The
                      FIRST start is external too: nothing in this component
                      starts a non-continuous scan on boot, so until the
                      automation actions land (follow-up PR) the radio stays
                      idle. start_scan() is called from code (e.g. an api
                      client-connected automation) so the single-core radio
                      can service WiFi in between scans.
"""

import esphome.codegen as cg
from esphome.components import ble_device_base, ln882h_ble, ota
from esphome.components.const import CONF_SCAN_PARAMETERS, CONF_WINDOW
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTIVE,
    CONF_CONTINUOUS,
    CONF_DURATION,
    CONF_ID,
    CONF_INTERVAL,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.types import ConfigType

CONF_LN882H_BLE_ID = "ln882h_ble_id"

DEPENDENCIES = ["ln882x"]
AUTO_LOAD = ["ble_device_base", "ln882h_ble"]
CODEOWNERS = ["@Bl00d-B0b"]

ln882h_ble_tracker_ns = cg.esphome_ns.namespace("ln882h_ble_tracker")
LN882HBLETracker = ln882h_ble_tracker_ns.class_(
    "LN882HBLETracker", ble_device_base.BLEHub, cg.Component
)


# Scan-parameter validation is shared across trackers (ble_device_base) —
# originally written here, lifted upstream in esphome#18003. LN882H SDK
# reference scan rate: 100 ms interval / 50 ms window (50 % duty).
SCAN_PARAMETERS_SCHEMA = ble_device_base.scan_parameters_schema(
    "100ms", window_default="50ms", supports_active=True
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LN882HBLETracker),
        cv.GenerateID(CONF_LN882H_BLE_ID): cv.use_id(ln882h_ble.LN882HBLE),
        cv.Optional(CONF_SCAN_PARAMETERS, default={}): SCAN_PARAMETERS_SCHEMA,
    }
).extend(cv.COMPONENT_SCHEMA)


# Runs at FINAL priority so every BLE sensor has registered through
# ble_device_base (and any tracker-owned listeners have been counted) before
# the StaticVector size is emitted. Same pattern as esp32_ble_tracker.
@coroutine_with_priority(CoroPriority.FINAL)
async def _emit_listener_count() -> None:
    count = ble_device_base.get_listener_count()
    if count > 0:
        cg.add_define("ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT", count)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_LN882H_BLE_ID])
    cg.add(var.set_parent(parent))
    # The tracker registers itself as a controller scan listener in setup();
    # request the codegen-sized StaticVector slot for it.
    ln882h_ble.request_scan_listener_slot()

    # Get notified when an OTA update starts, to pause scanning (esp32_ble_tracker parity)
    ota.request_ota_state_listeners()

    scan = config[CONF_SCAN_PARAMETERS]
    cg.add(var.set_scan_interval(ble_device_base.to_ble_units(scan[CONF_INTERVAL])))
    cg.add(var.set_scan_window(ble_device_base.to_ble_units(scan[CONF_WINDOW])))
    cg.add(var.set_scan_duration(scan[CONF_DURATION].total_milliseconds))
    cg.add(var.set_scan_active(scan[CONF_ACTIVE]))
    cg.add(var.set_scan_continuous(scan[CONF_CONTINUOUS]))

    CORE.add_job(_emit_listener_count)
