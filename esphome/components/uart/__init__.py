from dataclasses import dataclass
from logging import getLogger
import math
import re

from esphome import automation, pins
import esphome.codegen as cg
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_AFTER,
    CONF_BAUD_RATE,
    CONF_BYTES,
    CONF_DATA,
    CONF_DEBUG,
    CONF_DELIMITER,
    CONF_DIRECTION,
    CONF_DUMMY_RECEIVER,
    CONF_DUMMY_RECEIVER_ID,
    CONF_FLOW_CONTROL_PIN,
    CONF_ID,
    CONF_INVERT,
    CONF_LAMBDA,
    CONF_NUMBER,
    CONF_PORT,
    CONF_RX_BUFFER_SIZE,
    CONF_RX_PIN,
    CONF_SEQUENCE,
    CONF_TIMEOUT,
    CONF_TRIGGER_ID,
    CONF_TX_PIN,
    CONF_UART_ID,
    PLATFORM_HOST,
    PlatformFramework,
)
from esphome.core import CORE, ID, CoroPriority, coroutine_with_priority
import esphome.final_validate as fv
from esphome.yaml_util import make_data_base

_LOGGER = getLogger(__name__)

CODEOWNERS = ["@esphome/core"]
DOMAIN = "uart"


def AUTO_LOAD() -> list[str]:
    """Ideally, we would only auto-load socket only when wake_loop_on_rx is requested;
    however, AUTO_LOAD is examined before wake_loop_on_rx is set, so instead, since ESP32
    always uses socket select support in the main app, we'll just ensure it's loaded here.
    """
    if CORE.is_esp32:
        return ["socket"]
    return []


uart_ns = cg.esphome_ns.namespace("uart")
UARTComponent = uart_ns.class_("UARTComponent")

IDFUARTComponent = uart_ns.class_("IDFUARTComponent", UARTComponent, cg.Component)
ESP8266UartComponent = uart_ns.class_(
    "ESP8266UartComponent", UARTComponent, cg.Component
)
RP2040UartComponent = uart_ns.class_("RP2040UartComponent", UARTComponent, cg.Component)
LibreTinyUARTComponent = uart_ns.class_(
    "LibreTinyUARTComponent", UARTComponent, cg.Component
)
HostUartComponent = uart_ns.class_("HostUartComponent", UARTComponent, cg.Component)


NATIVE_UART_CLASSES = (
    str(IDFUARTComponent),
    str(ESP8266UartComponent),
    str(RP2040UartComponent),
    str(LibreTinyUARTComponent),
)

HOST_BAUD_RATES = [
    50,
    75,
    110,
    134,
    150,
    200,
    300,
    600,
    1200,
    1800,
    2400,
    4800,
    9600,
    19200,
    38400,
    57600,
    115200,
    230400,
    460800,
    500000,
    576000,
    921600,
    1000000,
    1152000,
    1500000,
    2000000,
    2500000,
    3000000,
    3500000,
    4000000,
]

UARTDevice = uart_ns.class_("UARTDevice")
UARTWriteAction = uart_ns.class_("UARTWriteAction", automation.Action)
UARTDebugger = uart_ns.class_("UARTDebugger", cg.Component, automation.Action)
UARTDummyReceiver = uart_ns.class_("UARTDummyReceiver", cg.Component)
MULTI_CONF = True
MULTI_CONF_NO_DEFAULT = True


@dataclass
class UARTData:
    """State data for UART component configuration generation."""

    wake_loop_on_rx: bool = False


def _get_data() -> UARTData:
    """Get UART component data from CORE.data."""
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = UARTData()
    return CORE.data[DOMAIN]


def request_wake_loop_on_rx() -> None:
    """Request that the UART wake the main loop when data is received.

    Components that need low-latency notification of incoming UART data
    should call this function during their code generation.
    This enables the RX event task which wakes the main loop when data arrives.
    """
    data = _get_data()
    if not data.wake_loop_on_rx:
        data.wake_loop_on_rx = True

        # UART RX event task uses wake_loop_threadsafe() to notify the main loop
        # Automatically enable the socket wake infrastructure when RX wake is requested
        from esphome.components import socket

        socket.require_wake_loop_threadsafe()


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


def validate_host_config(config):
    if CORE.is_host:
        if CONF_TX_PIN in config or CONF_RX_PIN in config:
            raise cv.Invalid(
                "TX and RX pins are not supported for UART on host platform."
            )
        if config[CONF_BAUD_RATE] not in HOST_BAUD_RATES:
            raise cv.Invalid(
                f"Host platform doesn't support baud rate {config[CONF_BAUD_RATE]}",
                path=[CONF_BAUD_RATE],
            )
    return config


def validate_rx_buffer_size(config):
    if CORE.is_esp32:
        # ESP32 UART hardware FIFO is 128 bytes (LP UART is 16 bytes, but we use 128 as safe minimum)
        # rx_buffer_size must be greater than the hardware FIFO length
        min_buffer_size = 128
        if config[CONF_RX_BUFFER_SIZE] <= min_buffer_size:
            _LOGGER.warning(
                "UART rx_buffer_size (%d bytes) is too small and must be greater than the hardware "
                "FIFO size (%d bytes). The buffer size will be automatically adjusted at runtime.",
                config[CONF_RX_BUFFER_SIZE],
                min_buffer_size,
            )
    return config


def _uart_declare_type(value):
    if CORE.is_esp8266:
        return cv.declare_id(ESP8266UartComponent)(value)
    if CORE.is_esp32:
        return cv.declare_id(IDFUARTComponent)(value)
    if CORE.is_rp2040:
        return cv.declare_id(RP2040UartComponent)(value)
    if CORE.is_libretiny:
        return cv.declare_id(LibreTinyUARTComponent)(value)
    if CORE.is_host:
        return cv.declare_id(HostUartComponent)(value)
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
CONF_RX_FULL_THRESHOLD = "rx_full_threshold"
CONF_RX_TIMEOUT = "rx_timeout"

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


def validate_port(value):
    if not re.match(r"^/(?:[^/]+/)[^/]+$", value):
        raise cv.Invalid("Port must be a valid device path")
    return value


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
            cv.Optional(CONF_FLOW_CONTROL_PIN): cv.All(
                cv.only_on_esp32, pins.internal_gpio_output_pin_schema
            ),
            cv.Optional(CONF_PORT): cv.All(validate_port, cv.only_on(PLATFORM_HOST)),
            cv.Optional(CONF_RX_BUFFER_SIZE, default=256): cv.validate_bytes,
            cv.Optional(CONF_RX_FULL_THRESHOLD): cv.All(
                cv.only_on_esp32, cv.validate_bytes, cv.int_range(min=1, max=120)
            ),
            cv.SplitDefault(CONF_RX_TIMEOUT, esp32=2): cv.All(
                cv.only_on_esp32, cv.validate_bytes, cv.int_range(min=0, max=92)
            ),
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
    cv.has_at_least_one_key(CONF_TX_PIN, CONF_RX_PIN, CONF_PORT),
    validate_host_config,
    validate_rx_buffer_size,
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
    if CONF_FLOW_CONTROL_PIN in config:
        flow_control_pin = await cg.gpio_pin_expression(config[CONF_FLOW_CONTROL_PIN])
        cg.add(var.set_flow_control_pin(flow_control_pin))
    if CONF_PORT in config:
        cg.add(var.set_name(config[CONF_PORT]))
    cg.add(var.set_rx_buffer_size(config[CONF_RX_BUFFER_SIZE]))
    if CORE.is_esp32:
        if CONF_RX_FULL_THRESHOLD not in config:
            # Calculate rx_full_threshold to be 10ms
            bytelength = config[CONF_DATA_BITS] + config[CONF_STOP_BITS] + 1
            if config[CONF_PARITY] != "NONE":
                bytelength += 1
            config[CONF_RX_FULL_THRESHOLD] = max(
                1,
                min(
                    120,
                    math.floor((config[CONF_BAUD_RATE] / (bytelength * 1000 / 10)) - 1),
                ),
            )
        cg.add(var.set_rx_full_threshold(config[CONF_RX_FULL_THRESHOLD]))
        cg.add(var.set_rx_timeout(config[CONF_RX_TIMEOUT]))
    cg.add(var.set_stop_bits(config[CONF_STOP_BITS]))
    cg.add(var.set_data_bits(config[CONF_DATA_BITS]))
    cg.add(var.set_parity(config[CONF_PARITY]))

    if CONF_DEBUG in config:
        await debug_to_code(config[CONF_DEBUG], var)

    CORE.add_job(final_step)


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
    uart_bus: str = CONF_UART_ID,
    baud_rate: int | None = None,
    require_tx: bool = False,
    require_rx: bool = False,
    data_bits: int | None = None,
    parity: str | None = None,
    stop_bits: int | None = None,
):
    def validate_baud_rate(value):
        if value != baud_rate:
            raise cv.Invalid(
                f"Component {name} requires baud rate {baud_rate} for the uart referenced by {uart_bus}"
            )
        return value

    def validate_pin(opt, device):
        def validator(value):
            if opt in device and not CORE.testing_mode:
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
                f"Component {name} requires {data_bits} data bits for the uart referenced by {uart_bus}"
            )
        return value

    def validate_parity(value):
        if value != parity:
            raise cv.Invalid(
                f"Component {name} requires parity {parity} for the uart referenced by {uart_bus}"
            )
        return value

    def validate_stop_bits(value):
        if value != stop_bits:
            raise cv.Invalid(
                f"Component {name} requires {stop_bits} stop bits for the uart referenced by {uart_bus}"
            )
        return value

    def validate_hub(hub_config):
        hub_schema = {}
        uart_id = hub_config[CONF_ID]
        uart_id_type_str = str(uart_id.type)
        devices = fv.full_config.get().data.setdefault(KEY_UART_DEVICES, {})
        device = devices.setdefault(uart_id, {})

        if require_tx and uart_id_type_str in NATIVE_UART_CLASSES:
            hub_schema[
                cv.Required(
                    CONF_TX_PIN,
                    msg=f"Component {name} requires uart referenced by {uart_bus} to declare a tx_pin",
                )
            ] = validate_pin(CONF_TX_PIN, device)
        if require_rx and uart_id_type_str in NATIVE_UART_CLASSES:
            hub_schema[
                cv.Required(
                    CONF_RX_PIN,
                    msg=f"Component {name} requires uart referenced by {uart_bus} to declare a rx_pin",
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
        {cv.Required(uart_bus): fv.id_declaration_match_schema(validate_hub)},
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
        # Generate static array in flash to avoid RAM copy
        arr_id = ID(f"{action_id}_data", is_declaration=True, type=cg.uint8)
        arr = cg.static_const_array(arr_id, cg.ArrayInitializer(*data))
        cg.add(var.set_data_static(arr, len(data)))
    return var


@coroutine_with_priority(CoroPriority.FINAL)
async def final_step():
    """Final code generation step to configure optional UART features."""
    if _get_data().wake_loop_on_rx:
        cg.add_define("USE_UART_WAKE_LOOP_ON_RX")


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "uart_component_esp_idf.cpp": {
            PlatformFramework.ESP32_IDF,
            PlatformFramework.ESP32_ARDUINO,
        },
        "uart_component_esp8266.cpp": {PlatformFramework.ESP8266_ARDUINO},
        "uart_component_host.cpp": {PlatformFramework.HOST_NATIVE},
        "uart_component_rp2040.cpp": {PlatformFramework.RP2040_ARDUINO},
        "uart_component_libretiny.cpp": {
            PlatformFramework.BK72XX_ARDUINO,
            PlatformFramework.RTL87XX_ARDUINO,
            PlatformFramework.LN882X_ARDUINO,
        },
    }
)
