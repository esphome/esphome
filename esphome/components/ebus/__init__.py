import voluptuous as vol
import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.const import (
    CONF_ID,
    CONF_TX_PIN,
    CONF_RX_PIN,
    CONF_ADDRESS,
    CONF_COMMAND,
    CONF_PAYLOAD,
    CONF_POSITION,
)


# TODO: send identification response when requested
# TODO: add debug mode that logs all messages on the bus
# TODO: investigate using UART component, but that does not seem to expose the UART NUM
# TODO: telegrams are always send to secondary right now, primary->primary communication
#       is possible according to the spec, but haven't found any need for it yet


CODEOWNERS = ["@guidoschreuder"]
DEPENDENCIES = ["esp32"]

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

CONF_TELEGRAM = "telegram"
CONF_SEND_POLL = "send_poll"
CONF_DECODE = "decode"

SYN = 0xAA
ESC = 0xA9


def validate_ebus_address(address):
    if address == SYN:
        raise vol.Invalid("SYN symbol (0xAA) is not a valid address")
    if address == ESC:
        raise vol.Invalid("ESC symbol (0xA9) is not a valid address")
    return cv.hex_uint8_t(address)


def is_primary_nibble(value):
    return (value & 0x0F) in {0x00, 0x01, 0x03, 0x07, 0x0F}


def validate_primary_address(value):
    """Validate that the config option is a valid ebus primary address."""
    if is_primary_nibble(value) and is_primary_nibble(value >> 4):
        return value & 0xFF
    raise vol.Invalid(f"'0x{value:02x}' is an invalid ebus primary address")


EbusComponent = ebus_ns.class_("EbusComponent", cg.Component)


def create_decode_schema(options):
    return cv.Schema(
        {
            cv.Optional(CONF_POSITION, default=0): cv.int_range(0, 15),
        }
    ).extend(options)


def create_telegram_schema(decode_options):
    return {
        cv.GenerateID(CONF_EBUS_ID): cv.use_id(EbusComponent),
        cv.Required(CONF_TELEGRAM): cv.Schema(
            {
                cv.Optional(CONF_SEND_POLL, default=False): cv.boolean,
                cv.Optional(CONF_ADDRESS): validate_ebus_address,
                cv.Required(CONF_COMMAND): cv.hex_uint16_t,
                cv.Required(CONF_PAYLOAD): cv.Schema([cv.hex_uint8_t]),
                cv.Optional(CONF_DECODE): create_decode_schema(decode_options),
            }
        ),
    }


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EbusComponent),
            cv.Optional(CONF_PRIMARY_ADDRESS): validate_primary_address,
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

    if CONF_PRIMARY_ADDRESS in config:
        cg.add(var.set_primary_address(config[CONF_PRIMARY_ADDRESS]))
    cg.add(var.set_max_tries(config[CONF_MAX_TRIES]))
    cg.add(var.set_max_lock_counter(config[CONF_MAX_LOCK_COUNTER]))
    cg.add(var.set_uart_num(config[CONF_UART][CONF_NUM]))
    cg.add(var.set_uart_tx_pin(config[CONF_UART][CONF_TX_PIN]))
    cg.add(var.set_uart_rx_pin(config[CONF_UART][CONF_RX_PIN]))
    cg.add(var.set_history_queue_size(config[CONF_HISTORY_QUEUE_SIZE]))
    cg.add(var.set_command_queue_size(config[CONF_COMMAND_QUEUE_SIZE]))
    cg.add(var.set_update_interval(config[CONF_POLL_INTERVAL].total_milliseconds))


def item_config(ebus, item, config):
    cg.add(item.set_parent(ebus))
    cg.add(ebus.add_item(item)),
    cg.add(item.set_send_poll(config[CONF_TELEGRAM][CONF_SEND_POLL]))
    if CONF_ADDRESS in config[CONF_TELEGRAM]:
        cg.add(item.set_address(config[CONF_TELEGRAM][CONF_ADDRESS]))
    cg.add(item.set_command(config[CONF_TELEGRAM][CONF_COMMAND]))
    cg.add(item.set_payload(config[CONF_TELEGRAM][CONF_PAYLOAD]))
    cg.add(
        item.set_response_read_position(
            config[CONF_TELEGRAM][CONF_DECODE][CONF_POSITION]
        )
    )
