"""BK72xx BLE Tracker — ESPHome BLE 5.x scanner for the BLE-5.x-capable
LibreTiny Beken chips (beken-72xx family).

Builds on the bk72xx_ble controller component (stack bring-up, BLE address,
scan primitives) and implements the platform-neutral ble_device_base BLEHub
contract: the shared BLE sensors (ble_presence, ble_rssi, ble_scanner,
bthome_mithermometer, xiaomi_*, …) bind to this tracker through
cv.use_id(BLEHub) with no BK-specific code.

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
from esphome.components import bk72xx_ble, ble_device_base, ota
from esphome.components.const import CONF_SCAN_PARAMETERS, CONF_WINDOW
import esphome.config_validation as cv
from esphome.const import CONF_CONTINUOUS, CONF_DURATION, CONF_ID, CONF_INTERVAL
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.types import ConfigType

CONF_BK72XX_BLE_ID = "bk72xx_ble_id"

DEPENDENCIES = ["bk72xx"]
AUTO_LOAD = ["ble_device_base", "bk72xx_ble"]
CODEOWNERS = ["@Bl00d-B0b"]

bk72xx_ble_tracker_ns = cg.esphome_ns.namespace("bk72xx_ble_tracker")
BK72xxBLETracker = bk72xx_ble_tracker_ns.class_(
    "BK72xxBLETracker", ble_device_base.BLEHub, cg.Component
)


# interval defaults to the BK reference scan rate — 100 ms with the shared 30 ms
# window, a 30 % duty cycle. Converted to the controller's 0.625 ms BLE units in
# to_code(). (LN882H's SDK recommends a different 100 / 50 ms = 50 %.)
SCAN_PARAMETERS_SCHEMA = ble_device_base.scan_parameters_schema("100ms")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BK72xxBLETracker),
        cv.GenerateID(CONF_BK72XX_BLE_ID): cv.use_id(bk72xx_ble.BK72xxBLE),
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

    parent = await cg.get_variable(config[CONF_BK72XX_BLE_ID])
    cg.add(var.set_parent(parent))
    # The tracker registers itself as a controller scan listener in setup();
    # request the codegen-sized StaticVector slot for it.
    bk72xx_ble.request_scan_listener_slot()

    # Get notified when an OTA update starts, to pause scanning (esp32_ble_tracker parity)
    ota.request_ota_state_listeners()

    scan = config[CONF_SCAN_PARAMETERS]
    cg.add(var.set_scan_interval(ble_device_base.to_ble_units(scan[CONF_INTERVAL])))
    cg.add(var.set_scan_window(ble_device_base.to_ble_units(scan[CONF_WINDOW])))
    cg.add(var.set_scan_duration(scan[CONF_DURATION].total_milliseconds))
    cg.add(var.set_scan_continuous(scan[CONF_CONTINUOUS]))

    CORE.add_job(_emit_listener_count)
