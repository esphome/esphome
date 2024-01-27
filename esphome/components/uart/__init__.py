from typing import Optional

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.yaml_util import make_data_base
from esphome import pins, automation
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_ID,
    CONF_NUMBER,
    CONF_RX_PIN,
    CONF_TX_PIN,
    CONF_UART_ID,
    CONF_DATA,
    CONF_RX_BUFFER_SIZE,
    CONF_INVERTED,
    CONF_INVERT,
    CONF_TRIGGER_ID,
    CONF_SEQUENCE,
    CONF_TIMEOUT,
    CONF_DEBUG,
    CONF_DIRECTION,
    CONF_AFTER,
    CONF_BYTES,
    CONF_DELIMITER,
    CONF_DUMMY_RECEIVER,
    CONF_DUMMY_RECEIVER_ID,
    CONF_LAMBDA,
)
from esphome.core import CORE

CODEOWNERS = ["@esphome/core"]
uart_ns = cg.esphome_ns.namespace("uart")
UARTComponent = uart_ns.class_("UARTComponent")

IDFUARTComponent = uart_ns.class_("IDFUARTComponent", UARTComponent, cg.Component)
ESP32ArduinoUARTComponent = uart_ns.class_(
    "ESP32ArduinoUARTComponent", UARTComponent, cg.Component
)
ESP8266UartComponent = uart_ns.class_(
    "ESP8266UartComponent", UARTComponent, cg.Component
)
RP2040UartComponent = uart_ns.class_("RP2040UartComponent", UARTComponent, cg.Component)
LibreTinyUARTComponent = uart_ns.class_(
    "LibreTinyUARTComponent", UARTComponent, cg.Component
)

UARTDevice = uart_ns.class_("UARTDevice")
UARTWriteAction = uart_ns.class_("UARTWriteAction", automation.Action)
UARTDebugger = uart_ns.class_("UARTDebugger", cg.Component, automation.Action)
UARTDummyReceiver = uart_ns.class_("UARTDummyReceiver", cg.Component)
MULTI_CONF = True


def validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, str):
        return value
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )


def validate_rx_pin(value):
    value = pins.internal_gpio_input_pin_schema(value)
    if CORE.is_esp8266 and value[CONF_NUMBER] >= 16:
        raise cv.Invalid("Pins GPIO16 and GPIO17 cannot be used as RX pins on ESP8266.")
    return value


def validate_invert_esp32(config):
    if (
        CORE.is_esp32
        and CORE.using_arduino
        and CONF_TX_PIN in config
        and CONF_RX_PIN in config
        and config[CONF_TX_PIN][CONF_INVERTED] != config[CONF_RX_PIN][CONF_INVERTED]
    ):
        raise cv.Invalid(
            "Different invert values for TX and RX pin are not supported for ESP32 when using Arduino."
        )
    return config


def _uart_declare_type(value):
    if CORE.is_esp8266:
        return cv.declare_id(ESP8266UartComponent)(value)
    if CORE.is_esp32:
        if CORE.using_arduino:
            return cv.declare_id(ESP32ArduinoUARTComponent)(value)
        if CORE.using_esp_idf:
            return cv.declare_id(IDFUARTComponent)(value)
    if CORE.is_rp2040:
        return cv.declare_id(RP2040UartComponent)(value)
    if CORE.is_libretiny:
        return cv.declare_id(LibreTinyUARTComponent)(value)
    raise NotImplementedError


UARTParityOptions = uart_ns.enum("UARTParityOptions")
UART_PARITY_OPTIONS = {
    "NONE": UARTParityOptions.UART_CONFIG_PARITY_NONE,
    "EVEN": UARTParityOptions.UART_CONFIG_PARITY_EVEN,
    "ODD": UARTParityOptions.UART_CONFIG_PARITY_ODD,
}

CONF_STOP_BITS = "stop_bits"
CONF_DATA_BITS = "data_bits"
CONF_PARITY = "parity"

UARTDirection = uart_ns.enum("UARTDirection")
UART_DIRECTIONS = {
    "RX": UARTDirection.UART_DIRECTION_RX,
    "TX": UARTDirection.UART_DIRECTION_TX,
    "BOTH": UARTDirection.UART_DIRECTION_BOTH,
}

# The reason for having CONF_BYTES at 150 by default:
#
# The log message buffer size is 512 bytes by default. About 35 bytes are
# used for the log prefix. That leaves us with 477 bytes for logging data.
# The default log output is hex, which uses 3 characters per represented
# byte (2 hex chars + 1 separator). That means that 477 / 3 = 159 bytes
# can be represented in a single log line. Using 150, because people love
# round numbers.
AFTER_DEFAULTS = {CONF_BYTES: 150, CONF_TIMEOUT: "100ms"}

# By default, log in hex format when no specific sequence is provided.
DEFAULT_DEBUG_OUTPUT = "UARTDebug::log_hex(direction, bytes, ':');"
DEFAULT_SEQUENCE = [{CONF_LAMBDA: make_data_base(DEFAULT_DEBUG_OUTPUT)}]


def maybe_empty_debug(value):
    if value is None:
        value = {}
    return DEBUG_SCHEMA(value)


DEBUG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(UARTDebugger),
        cv.Optional(CONF_DIRECTION, default="BOTH"): cv.enum(
            UART_DIRECTIONS, upper=True
        ),
        cv.Optional(CONF_AFTER, default=AFTER_DEFAULTS): cv.Schema(
            {
                cv.Optional(
                    CONF_BYTES, default=AFTER_DEFAULTS[CONF_BYTES]
                ): cv.validate_bytes,
                cv.Optional(
                    CONF_TIMEOUT, default=AFTER_DEFAULTS[CONF_TIMEOUT]
                ): cv.positive_time_period_milliseconds,
                cv.Optional(CONF_DELIMITER): cv.templatable(validate_raw_data),
            }
        ),
        cv.Optional(
            CONF_SEQUENCE, default=DEFAULT_SEQUENCE
        ): automation.validate_automation(),
        cv.Optional(CONF_DUMMY_RECEIVER, default=False): cv.boolean,
        cv.GenerateID(CONF_DUMMY_RECEIVER_ID): cv.declare_id(UARTDummyReceiver),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): _uart_declare_type,
            cv.Required(CONF_BAUD_RATE): cv.int_range(min=1),
            cv.Optional(CONF_TX_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_RX_PIN): validate_rx_pin,
            cv.Optional(CONF_RX_BUFFER_SIZE, default=256): cv.validate_bytes,
            cv.Optional(CONF_STOP_BITS, default=1): cv.one_of(1, 2, int=True),
            cv.Optional(CONF_DATA_BITS, default=8): cv.int_range(min=5, max=8),
            cv.Optional(CONF_PARITY, default="NONE"): cv.enum(
                UART_PARITY_OPTIONS, upper=True
            ),
            cv.Optional(CONF_INVERT): cv.invalid(
                "This option has been removed. Please instead use invert in the tx/rx pin schemas."
            ),
            cv.Optional(CONF_DEBUG): maybe_empty_debug,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_at_least_one_key(CONF_TX_PIN, CONF_RX_PIN),
    validate_invert_esp32,
)


async def debug_to_code(config, parent):
    trigger = cg.new_Pvariable(config[CONF_TRIGGER_ID], parent)
    await cg.register_component(trigger, config)
    for action in config[CONF_SEQUENCE]:
        await automation.build_automation(
            trigger,
            [(UARTDirection, "direction"), (cg.std_vector.template(cg.uint8), "bytes")],
            action,
        )
    cg.add(trigger.set_direction(config[CONF_DIRECTION]))
    after = config[CONF_AFTER]
    cg.add(trigger.set_after_bytes(after[CONF_BYTES]))
    cg.add(trigger.set_after_timeout(after[CONF_TIMEOUT]))
    if CONF_DELIMITER in after:
        data = after[CONF_DELIMITER]
        if isinstance(data, bytes):
            data = list(data)
        for byte in after[CONF_DELIMITER]:
            cg.add(trigger.add_delimiter_byte(byte))
    if config[CONF_DUMMY_RECEIVER]:
        dummy = cg.new_Pvariable(config[CONF_DUMMY_RECEIVER_ID], parent)
        await cg.register_component(dummy, {})
    cg.add_define("USE_UART_DEBUGGER")


async def to_code(config):
    cg.add_global(uart_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))

    if CONF_TX_PIN in config:
        tx_pin = await cg.gpio_pin_expression(config[CONF_TX_PIN])
        cg.add(var.set_tx_pin(tx_pin))
    if CONF_RX_PIN in config:
        rx_pin = await cg.gpio_pin_expression(config[CONF_RX_PIN])
        cg.add(var.set_rx_pin(rx_pin))
    cg.add(var.set_rx_buffer_size(config[CONF_RX_BUFFER_SIZE]))
    cg.add(var.set_stop_bits(config[CONF_STOP_BITS]))
    cg.add(var.set_data_bits(config[CONF_DATA_BITS]))
    cg.add(var.set_parity(config[CONF_PARITY]))

    if CONF_DEBUG in config:
        await debug_to_code(config[CONF_DEBUG], var)


# A schema to use for all UART devices, all UART integrations must extend this!
UART_DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_UART_ID): cv.use_id(UARTComponent),
    }
)

KEY_UART_DEVICES = "uart_devices"


def final_validate_device_schema(
    name: str,
    *,
    baud_rate: Optional[int] = None,
    require_tx: bool = False,
    require_rx: bool = False,
    data_bits: Optional[int] = None,
    parity: Optional[str] = None,
    stop_bits: Optional[int] = None,
):
    def validate_baud_rate(value):
        if value != baud_rate:
            raise cv.Invalid(
                f"Component {name} requires baud rate {baud_rate} for the uart bus"
            )
        return value

    def validate_pin(opt, device):
        def validator(value):
            if opt in device:
                raise cv.Invalid(
                    f"The uart {opt} is used both by {name} and {device[opt]}, "
                    f"but can only be used by one. Please create a new uart bus for {name}."
                )
            device[opt] = name
            return value

        return validator

    def validate_data_bits(value):
        if value != data_bits:
            raise cv.Invalid(
                f"Component {name} requires {data_bits} data bits for the uart bus"
            )
        return value

    def validate_parity(value):
        if value != parity:
            raise cv.Invalid(
                f"Component {name} requires parity {parity} for the uart bus"
            )
        return value

    def validate_stop_bits(value):
        if value != stop_bits:
            raise cv.Invalid(
                f"Component {name} requires {stop_bits} stop bits for the uart bus"
            )
        return value

    def validate_hub(hub_config):
        hub_schema = {}
        uart_id = hub_config[CONF_ID]
        devices = fv.full_config.get().data.setdefault(KEY_UART_DEVICES, {})
        device = devices.setdefault(uart_id, {})

        if require_tx:
            hub_schema[
                cv.Required(
                    CONF_TX_PIN,
                    msg=f"Component {name} requires this uart bus to declare a tx_pin",
                )
            ] = validate_pin(CONF_TX_PIN, device)
        if require_rx:
            hub_schema[
                cv.Required(
                    CONF_RX_PIN,
                    msg=f"Component {name} requires this uart bus to declare a rx_pin",
                )
            ] = validate_pin(CONF_RX_PIN, device)
        if baud_rate is not None:
            hub_schema[cv.Required(CONF_BAUD_RATE)] = validate_baud_rate
        if data_bits is not None:
            hub_schema[cv.Required(CONF_DATA_BITS)] = validate_data_bits
        if parity is not None:
            hub_schema[cv.Required(CONF_PARITY)] = validate_parity
        if stop_bits is not None:
            hub_schema[cv.Required(CONF_STOP_BITS)] = validate_stop_bits
        return cv.Schema(hub_schema, extra=cv.ALLOW_EXTRA)(hub_config)

    return cv.Schema(
        {cv.Required(CONF_UART_ID): fv.id_declaration_match_schema(validate_hub)},
        extra=cv.ALLOW_EXTRA,
    )


async def register_uart_device(var, config):
    """Register a UART device, setting up all the internal values.

    This is a coroutine, you need to await it with a 'yield' expression!
    """
    parent = await cg.get_variable(config[CONF_UART_ID])
    cg.add(var.set_uart_parent(parent))


@automation.register_action(
    "uart.write",
    UARTWriteAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(UARTComponent),
            cv.Required(CONF_DATA): cv.templatable(validate_raw_data),
        },
        key=CONF_DATA,
    ),
)
async def uart_write_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    data = config[CONF_DATA]
    if isinstance(data, bytes):
        data = list(data)

    if cg.is_template(data):
        templ = await cg.templatable(data, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_data_template(templ))
    else:
        cg.add(var.set_data_static(data))
    return var
