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


def to_ble_units(value: cv.TimePeriod) -> int:
    """Convert a scan time to the controller's 0.625 ms units.

    Used by both validation and codegen so what is validated is exactly what is
    programmed — the truncation here is what makes the duty-cycle check below
    meaningful.
    """
    return value.total_microseconds // 625


def validate_scan_parameters(config: ConfigType) -> ConfigType:
    """Reject impossible window/interval/duration combinations at config time.

    Mirrors esp32_ble_tracker: the controller cannot scan for longer than the
    interval, and a too-short duration would end the scan period almost
    immediately. Catching it here gives a clear error instead of a runtime
    controller failure and the 1/sec retry loop.
    """
    duration = config[CONF_DURATION]
    interval = config[CONF_INTERVAL]
    window = config[CONF_WINDOW]

    if window > interval:
        raise cv.Invalid(
            f"Scan window ({window}) needs to be smaller than scan interval ({interval})"
        )

    # BLE scan interval/window are programmed in 0.625 ms units as a 16-bit value; the
    # controller only accepts 2.5 ms .. 10240 ms (0x0004 .. 0x4000). Reject out-of-range
    # values here instead of letting the unit conversion silently overflow.
    for name, value in (("interval", interval), ("window", window)):
        if value.total_microseconds < 2500 or value.total_microseconds > 10_240_000:
            raise cv.Invalid(
                f"Scan {name} ({value}) must be between 2.5 ms and 10240 ms"
            )

    # Validate what actually reaches the controller: both values are truncated to
    # whole 0.625 ms units, so a window/interval pair that differs by less than one
    # unit collapses to the same value — silently programming a 100 % duty cycle
    # (radio permanently on) from a config that asked for less.
    interval_units = to_ble_units(interval)
    window_units = to_ble_units(window)
    if window_units == interval_units and window < interval:
        raise cv.Invalid(
            f"Scan window ({window}) and interval ({interval}) both round to "
            f"{interval_units} x 0.625 ms, which the controller scans at a 100 % duty "
            f"cycle. Separate them by at least 0.625 ms."
        )

    if interval.total_microseconds * 3 > duration.total_microseconds:
        raise cv.Invalid(
            f"Scan duration ({duration}) must cover at least three scan intervals "
            f"({interval}): the scanner listens on one of the three BLE advertising "
            f"channels per interval, so a shorter duration can miss devices entirely."
        )

    return config


SCAN_PARAMETERS_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_DURATION, default="5min"): cv.positive_time_period_seconds,
            # interval/window default to the BK reference scan rate — 100 ms / 30 ms,
            # a 30 % duty cycle. Converted to the controller's 0.625 ms BLE units in
            # to_code(). (LN882H's SDK recommends a different 100 / 50 ms = 50 %.)
            cv.Optional(CONF_INTERVAL, default="100ms"): cv.positive_time_period,
            cv.Optional(CONF_WINDOW, default="30ms"): cv.positive_time_period,
            cv.Optional(CONF_CONTINUOUS, default=True): cv.boolean,
        }
    ),
    validate_scan_parameters,
)

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

    # Get notified when an OTA update starts, to pause scanning (esp32_ble_tracker parity)
    ota.request_ota_state_listeners()

    scan = config[CONF_SCAN_PARAMETERS]
    cg.add(var.set_scan_interval(to_ble_units(scan[CONF_INTERVAL])))
    cg.add(var.set_scan_window(to_ble_units(scan[CONF_WINDOW])))
    cg.add(var.set_scan_duration(scan[CONF_DURATION].total_milliseconds))
    cg.add(var.set_scan_continuous(scan[CONF_CONTINUOUS]))

    CORE.add_job(_emit_listener_count)
