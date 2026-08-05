"""LN882H BLE — BLE controller support for the LN882H LibreTiny chips.

The platform analog of esp32_ble / rp2040_ble: owns the LN882H BLE stack
bring-up and the controller BLE address. Consumers (ln882h_ble_tracker) build
on this component and contain no SDK calls of their own.

The BLE stack is compiled and linked by the LibreTiny lightning-ln882h builder
when CFG_SUPPORT_BLE=1 is set via custom_options.proj_config#h, which this
component does (LibreTiny v1.13.0+).
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ENABLE_ON_BOOT, CONF_ID
from esphome.types import ConfigType

DEPENDENCIES = ["ln882x"]
CODEOWNERS = ["@Bl00d-B0b"]

ln882h_ble_ns = cg.esphome_ns.namespace("ln882h_ble")
LN882HBLE = ln882h_ble_ns.class_("LN882HBLE", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LN882HBLE),
        # Default off: on the single-core LN882H, bringing the BLE stack up during
        # boot competes with the WiFi connection handshake. Consumers enable the
        # stack lazily on first use (e.g. the tracker's first scan start).
        cv.Optional(CONF_ENABLE_ON_BOOT, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


# Once per registered scan listener; sizes the controller's StaticVector
# listener storage.
request_scan_listener_slot = cg.slot_counter("LN882H_BLE_SCAN_LISTENER_COUNT")


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_enable_on_boot(config[CONF_ENABLE_ON_BOOT]))

    # Enable the BLE stack in the build. The LibreTiny lightning-ln882h builder
    # gates the BLE stack sources and libraries on CFG_SUPPORT_BLE, read from
    # the custom option key "proj_config#h" (the '#h' maps to proj_config.h)
    # before the stock header's default of 0. A list is used so any other
    # component adding to this key concatenates with it rather than replacing
    # it (add_platformio_option only merges when both values are lists).
    cg.add_platformio_option("custom_options.proj_config#h", ["CFG_SUPPORT_BLE=1"])

    cg.add_define("USE_LN882H_BLE")
