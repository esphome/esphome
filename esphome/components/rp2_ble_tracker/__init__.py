"""BLE scanner for the Raspberry Pi Pico W / Pico 2 W (BLEHub on rp2040_ble).

Scan modes:
  continuous: true  — scan runs forever; never stops automatically.
  continuous: false — a started scan runs for `duration`, then stops. The first
                      start is external too; nothing starts a non-continuous
                      scan on boot. Until start/stop automation actions land
                      (follow-up PR), starting means a lambda:
                      `id(my_tracker).start_scan();`.
"""

import esphome.codegen as cg
from esphome.components import ble_device_base, ota, rp2040_ble
from esphome.components.const import CONF_SCAN_PARAMETERS, CONF_WINDOW
from esphome.components.rp2040_ble import CONF_RP2040_BLE_ID
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTIVE,
    CONF_CONTINUOUS,
    CONF_DURATION,
    CONF_ID,
    CONF_INTERVAL,
)
from esphome.types import ConfigType

DEPENDENCIES = ["rp2"]
AUTO_LOAD = ["ble_device_base", "rp2040_ble"]
CODEOWNERS = ["@bdraco"]

ble_device_base.register_hub_provider("rp2_ble_tracker")

rp2_ble_tracker_ns = cg.esphome_ns.namespace("rp2_ble_tracker")
RP2BLETracker = rp2_ble_tracker_ns.class_(
    "RP2BLETracker", ble_device_base.BLEHub, cg.Component
)


# interval defaults to 100 ms with the shared 30 ms window, a 30 % duty cycle —
# the same defaults as bk72xx_ble_tracker, leaving the radio mostly free for
# WiFi on the shared CYW43. Converted to the controller's 0.625 ms BLE units in
# to_code(). `active` defaults on for esp32_ble_tracker parity; it adds scan
# request TX and roughly doubles the reports through the queue, so
# `active: false` is the lighter choice when scan response data is not needed.
SCAN_PARAMETERS_SCHEMA = ble_device_base.scan_parameters_schema("100ms")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RP2BLETracker),
        cv.GenerateID(CONF_RP2040_BLE_ID): cv.use_id(rp2040_ble.RP2040BLE),
        cv.Optional(CONF_SCAN_PARAMETERS, default={}): SCAN_PARAMETERS_SCHEMA,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    # Selects the BLEHub alias arm in ble_device_base/ble_hub_impl.h.
    cg.add_define("USE_RP2_BLE_TRACKER")
    # Compiles the shared adv + scan-response merge (BTstack delivers the pair
    # as separate reports).
    cg.add_define("USE_BLE_SCAN_RESPONSE_MERGER")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_RP2040_BLE_ID])
    cg.add(var.set_parent(parent))
    # The tracker registers itself as a controller scan listener in setup();
    # request the codegen-sized StaticVector slot for it.
    rp2040_ble.request_scan_listener_slot()

    # Get notified when an OTA update starts, to pause scanning (esp32_ble_tracker parity)
    ota.request_ota_state_listeners()

    scan = config[CONF_SCAN_PARAMETERS]
    cg.add(var.set_scan_interval(ble_device_base.to_ble_units(scan[CONF_INTERVAL])))
    cg.add(var.set_scan_window(ble_device_base.to_ble_units(scan[CONF_WINDOW])))
    cg.add(var.set_scan_duration(scan[CONF_DURATION].total_milliseconds))
    cg.add(var.set_scan_active(scan[CONF_ACTIVE]))
    cg.add(var.set_scan_continuous(scan[CONF_CONTINUOUS]))
