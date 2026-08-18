from collections.abc import Callable, MutableMapping

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ENABLE_ON_BOOT, CONF_ID
from esphome.core import CORE
from esphome.types import ConfigType

DEPENDENCIES = ["rp2"]
CODEOWNERS = ["@bdraco"]

CONF_RP2040_BLE_ID = "rp2040_ble_id"

KEY_RP2040_BLE = "rp2040_ble"
KEY_USED_CONNECTION_SLOTS = "used_connection_slots"

# Hard platform cap on concurrent GATT connections: the BTstack pool overrides
# in btstack_memory.cpp are sized from ESPHOME_BLE_GATT_CLIENT_COUNT with this
# as the ceiling. 3 matches the esp32 default and stays within the
# controller's resources (MAX_NR_CONTROLLER_ACL_BUFFERS 3).
MAX_CONNECTIONS = 3

rp2040_ble_ns = cg.esphome_ns.namespace("rp2040_ble")
RP2040BLE = rp2040_ble_ns.class_("RP2040BLE", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RP2040BLE),
        cv.Optional(CONF_ENABLE_ON_BOOT, default=True): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


def _validate_board(config: ConfigType) -> ConfigType:
    from esphome.components.rp2 import board_has_wifi, get_board

    if not board_has_wifi():
        raise cv.Invalid(
            f"Board '{get_board()}' does not have Bluetooth support (no CYW43 wireless "
            f"chip). Use a board like 'rpipicow' or 'rpipico2w'."
        )
    return config


def consume_connection_slots(
    value: int, consumer: str
) -> Callable[[MutableMapping], MutableMapping]:
    """Reserve BLE connection slots for a component (the esp32_ble pattern);
    the total is checked against MAX_CONNECTIONS in final validation."""

    def _consume_connection_slots(config: MutableMapping) -> MutableMapping:
        data: dict = CORE.data.setdefault(KEY_RP2040_BLE, {})
        slots: list[str] = data.setdefault(KEY_USED_CONNECTION_SLOTS, [])
        slots.extend([consumer] * value)
        return config

    return _consume_connection_slots


def validate_connection_slots() -> None:
    """Fail when consumers claimed more slots than the platform cap."""
    # Skip in testing mode to allow component grouping (esp32_ble parity).
    if CORE.testing_mode:
        return
    used = CORE.data.get(KEY_RP2040_BLE, {}).get(KEY_USED_CONNECTION_SLOTS, [])
    if len(used) > MAX_CONNECTIONS:
        raise cv.Invalid(
            f"BLE components require {len(used)} connection slots but the "
            f"rp2 maximum is {MAX_CONNECTIONS}. "
            f"Components: {', '.join(used)}"
        )


def _final_validate(config: ConfigType) -> None:
    _validate_board(config)
    validate_connection_slots()


FINAL_VALIDATE_SCHEMA = _final_validate


# Once per registered scan listener; sizes the controller's StaticVector
# listener storage.
request_scan_listener_slot = cg.slot_counter("RP2040_BLE_SCAN_LISTENER_COUNT")

# The four btstack_memory accessors whose static pools are baked into the
# prebuilt liblwip-bt.a; every internal use crosses an object boundary in the
# archive, so --wrap intercepts them all (see btstack_memory.cpp).
_BTSTACK_POOL_SYMBOLS = (
    "btstack_memory_gatt_client_get",
    "btstack_memory_gatt_client_free",
    "btstack_memory_hci_connection_get",
    "btstack_memory_hci_connection_free",
)


def add_btstack_pool_overrides() -> None:
    """Emit the --wrap flags that swap the prebuilt BTstack pools for the
    ESPHOME_BLE_GATT_CLIENT_COUNT-sized ones in btstack_memory.cpp. Called by
    bluetooth_connection when a second GATT backend registers; idempotent
    (build flags are a set)."""
    for symbol in _BTSTACK_POOL_SYMBOLS:
        cg.add_build_flag(f"-Wl,--wrap={symbol}")


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_enable_on_boot(config[CONF_ENABLE_ON_BOOT]))

    # Enable Bluetooth in the arduino-pico build
    # This switches linking from liblwip.a to liblwip-bt.a and defines
    # ENABLE_CLASSIC, ENABLE_BLE, CYW43_ENABLE_BLUETOOTH
    cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_ENABLE_BLUETOOTH")

    cg.add_define("USE_RP2040_BLE")
