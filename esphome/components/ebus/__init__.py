import voluptuous as vol
import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.const import (
    CONF_ID,
    CONF_TX_PIN,
    CONF_RX_PIN,
)

CODEOWNERS = ["@guidoschreuder"]

ebus_ns = cg.esphome_ns.namespace("ebus")

CONF_EBUS_ID = "ebus_id"
CONF_PRIMARY_ADDRESS = "primary_address"
CONF_MAX_TRIES = "max_tries"
CONF_MAX_LOCK_COUNTER = "max_lock_counter"
CONF_HISTORY_QUEUE_SIZE = "history_queue_size"
CONF_COMMAND_QUEUE_SIZE = "command_queue_size"
CONF_POLL_INTERVAL = "poll_interval"
CONF_UART = "uart"
CONF_NUM = "num"


def is_primary_nibble(value):
    return (value & 0x0F) in {0x00, 0x01, 0x03, 0x07, 0x0F}


def validate_primary_address(value):
    """Validate that the config option is a valid ebus primary address."""
    if is_primary_nibble(value) and is_primary_nibble(value >> 4):
        return value & 0xFF
    raise vol.Invalid(f"'0x{value:02x}' is an invalid ebus primary address")


EbusComponent = ebus_ns.class_("EbusComponent", cg.Component)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EbusComponent),
            cv.Required(CONF_PRIMARY_ADDRESS): validate_primary_address,
            cv.Optional(CONF_MAX_TRIES, default=2): cv.hex_uint8_t,
            cv.Optional(CONF_MAX_LOCK_COUNTER, default=4): cv.hex_uint8_t,
            cv.Optional(CONF_HISTORY_QUEUE_SIZE, default=20): cv.uint8_t,
            cv.Optional(CONF_COMMAND_QUEUE_SIZE, default=10): cv.uint8_t,
            cv.Optional(CONF_POLL_INTERVAL, default="30s"): cv.time_period,
            cv.Required(CONF_UART): cv.Schema(
                {
                    cv.Required(CONF_NUM): cv.uint8_t,
                    cv.Required(CONF_TX_PIN): cv.uint8_t,
                    cv.Required(CONF_RX_PIN): cv.uint8_t,
                }
            ),
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_primary_address(config[CONF_PRIMARY_ADDRESS]))
    cg.add(var.set_max_tries(config[CONF_MAX_TRIES]))
    cg.add(var.set_max_lock_counter(config[CONF_MAX_LOCK_COUNTER]))
    cg.add(var.set_uart_num(config[CONF_UART][CONF_NUM]))
    cg.add(var.set_uart_tx_pin(config[CONF_UART][CONF_TX_PIN]))
    cg.add(var.set_uart_rx_pin(config[CONF_UART][CONF_RX_PIN]))
    cg.add(var.set_history_queue_size(config[CONF_HISTORY_QUEUE_SIZE]))
    cg.add(var.set_command_queue_size(config[CONF_COMMAND_QUEUE_SIZE]))
    cg.add(var.set_update_interval(config[CONF_POLL_INTERVAL].total_milliseconds))
