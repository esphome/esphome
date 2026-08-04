"""LN882H BLE scanner implementing the ble_device_base BLEHub contract on
top of the ln882h_ble controller. With continuous: false nothing scans until
an explicit start_scan() call."""

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
from esphome.types import ConfigType

CONF_LN882H_BLE_ID = "ln882h_ble_id"

DEPENDENCIES = ["ln882x"]
AUTO_LOAD = ["ble_device_base", "ln882h_ble"]
CODEOWNERS = ["@Bl00d-B0b"]

ln882h_ble_tracker_ns = cg.esphome_ns.namespace("ln882h_ble_tracker")
LN882HBLETracker = ln882h_ble_tracker_ns.class_(
    "LN882HBLETracker", ble_device_base.BLEHub, cg.Component
)


# LN882H SDK reference scan rate: 100 ms interval / 50 ms window (50 % duty).
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
