from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32, uart
from esphome.components.const import (
    CONF_DATA_BITS,
    CONF_PARITY,
    CONF_RECONNECT_INTERVAL,
    CONF_STOP_BITS,
)
from esphome.components.esp32 import (
    VARIANT_ESP32,
    add_idf_sdkconfig_option,
    request_bluetooth,
    request_software_coexistence,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_CHANNEL,
    CONF_DEBUG,
    CONF_ENABLE_ON_BOOT,
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_NAME,
    CONF_RX_BUFFER_SIZE,
    CONF_TX_BUFFER_SIZE,
)
from esphome.core import CORE
from esphome.types import ConfigType

AUTO_LOAD = ["uart"]
CODEOWNERS = ["@MatthiasNeuhaus"]
# The Bluedroid SPP and GAP callbacks are global and the controller runs in
# BR/EDR-only mode, so this cannot share the radio with the BLE components.
CONFLICTS_WITH = ["esp32_ble", "esp32_ble_tracker", "bluetooth_proxy"]
DEPENDENCIES = ["esp32"]
DOMAIN = "bt_spp"

CONF_BT_SPP_ID = "bt_spp_id"

bt_spp_ns = cg.esphome_ns.namespace("bt_spp")
BtSppComponent = bt_spp_ns.class_("BtSppComponent", uart.UARTComponent, cg.Component)

# Bluetooth names are up to 248 bytes; keep them readable on a phone screen.
MAX_NAME_LENGTH = 64
CONF_PIN_CODE = "pin_code"
# Legacy pairing PIN codes are 1 to 16 bytes long.
MAX_PIN_LENGTH = 16

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.All(cv.declare_id(BtSppComponent), uart.mark_unclocked),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_CHANNEL): cv.int_range(min=1, max=30),
            cv.Optional(CONF_PIN_CODE): cv.sensitive(
                cv.All(
                    cv.string_strict,
                    cv.Length(min=1),
                    cv.ByteLength(max=MAX_PIN_LENGTH),
                )
            ),
            cv.Optional(CONF_NAME): cv.All(
                cv.string_strict, cv.Length(min=1, max=MAX_NAME_LENGTH)
            ),
            cv.Optional(CONF_ENABLE_ON_BOOT, default=True): cv.boolean,
            cv.Optional(CONF_RECONNECT_INTERVAL, default="10s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=cv.TimePeriod(seconds=1)),
            ),
            cv.Optional(CONF_RX_BUFFER_SIZE, default=512): cv.All(
                cv.validate_bytes, cv.int_range(min=64, max=16384)
            ),
            cv.Optional(CONF_TX_BUFFER_SIZE, default=256): cv.All(
                cv.validate_bytes, cv.int_range(min=32, max=4096)
            ),
            # The link is not clocked. These only satisfy UART devices that check them.
            cv.Optional(CONF_BAUD_RATE, default=9600): cv.int_range(min=1),
            cv.Optional(CONF_DATA_BITS, default=8): cv.int_range(min=5, max=8),
            cv.Optional(CONF_PARITY, default="NONE"): cv.enum(
                uart.UART_PARITY_OPTIONS, upper=True
            ),
            cv.Optional(CONF_STOP_BITS, default=1): cv.one_of(1, 2, int=True),
            cv.Optional(CONF_DEBUG): uart.maybe_empty_debug,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    esp32.only_on_variant(supported=[VARIANT_ESP32]),
)


async def to_code(config: ConfigType) -> None:
    rx_size = config[CONF_RX_BUFFER_SIZE]
    tx_size = config[CONF_TX_BUFFER_SIZE]
    # One static block: the RX stream buffer needs one byte more than it holds, then the TX queue.
    buffer = cg.static_buffer(config[CONF_ID], cg.uint8, rx_size + 1 + tx_size)
    var = cg.new_Pvariable(
        config[CONF_ID], config[CONF_MAC_ADDRESS].as_hex, buffer, rx_size, tx_size
    )
    await cg.register_component(var, config)

    cg.add_optional(var.set_channel, config.get(CONF_CHANNEL))
    cg.add_optional(var.set_pin_code, config.get(CONF_PIN_CODE))
    cg.add_optional(var.set_name, config.get(CONF_NAME))
    if not config[CONF_ENABLE_ON_BOOT]:
        cg.add(var.set_enable_on_boot(False))
    cg.add(var.set_reconnect_interval(config[CONF_RECONNECT_INTERVAL]))
    cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))
    cg.add(var.set_data_bits(config[CONF_DATA_BITS]))
    cg.add(var.set_stop_bits(config[CONF_STOP_BITS]))
    cg.add(var.set_parity(config[CONF_PARITY]))
    if (debug := config.get(CONF_DEBUG)) is not None:
        # The default debug lambda names UARTDebug without its namespace.
        cg.add_global(uart.uart_ns.using)
        await uart.debug_to_code(debug, var)

    request_bluetooth()
    if "wifi" in CORE.config:
        request_software_coexistence()
    # Bluetooth Classic only: BLE stays off and its controller memory is released.
    add_idf_sdkconfig_option("CONFIG_BT_BLUEDROID_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BT_CLASSIC_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BT_SPP_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BT_BLE_ENABLED", False)
    add_idf_sdkconfig_option("CONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY", True)
    add_idf_sdkconfig_option("CONFIG_BTDM_CTRL_MODE_BLE_ONLY", False)
    add_idf_sdkconfig_option("CONFIG_BTDM_CTRL_MODE_BTDM", False)


BT_SPP_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(BtSppComponent)}
)

automation.register_apply_action(
    "bt_spp.connect", BT_SPP_ACTION_SCHEMA, automation.ApplyCall("connect()")
)
automation.register_apply_action(
    "bt_spp.disconnect", BT_SPP_ACTION_SCHEMA, automation.ApplyCall("disconnect()")
)
automation.register_apply_condition(
    "bt_spp.is_connected", BT_SPP_ACTION_SCHEMA, "is_connected()"
)
