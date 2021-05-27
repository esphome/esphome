import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins, automation
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_ID,
    CONF_RX_PIN,
    CONF_TX_PIN,
    CONF_UART_ID,
    CONF_DATA,
    CONF_RX_BUFFER_SIZE,
    CONF_INVERT,
)
from esphome.core import CORE

CODEOWNERS = ["@esphome/core"]
uart_ns = cg.esphome_ns.namespace("uart")
UARTComponent = uart_ns.class_("UARTComponent", cg.Component)
UARTDevice = uart_ns.class_("UARTDevice")
UARTWriteAction = uart_ns.class_("UARTWriteAction", automation.Action)
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
    value = pins.input_pin(value)
    if CORE.is_esp8266 and value >= 16:
        raise cv.Invalid("Pins GPIO16 and GPIO17 cannot be used as RX pins on ESP8266.")
    return value


UARTParityOptions = uart_ns.enum("UARTParityOptions")
UART_PARITY_OPTIONS = {
    "NONE": UARTParityOptions.UART_CONFIG_PARITY_NONE,
    "EVEN": UARTParityOptions.UART_CONFIG_PARITY_EVEN,
    "ODD": UARTParityOptions.UART_CONFIG_PARITY_ODD,
}

CONF_STOP_BITS = "stop_bits"
CONF_DATA_BITS = "data_bits"
CONF_PARITY = "parity"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UARTComponent),
            cv.Required(CONF_BAUD_RATE): cv.int_range(min=1),
            cv.Optional(CONF_TX_PIN): pins.output_pin,
            cv.Optional(CONF_RX_PIN): validate_rx_pin,
            cv.Optional(CONF_RX_BUFFER_SIZE, default=256): cv.validate_bytes,
            cv.SplitDefault(CONF_INVERT, esp32=False): cv.All(
                cv.only_on_esp32, cv.boolean
            ),
            cv.Optional(CONF_STOP_BITS, default=1): cv.one_of(1, 2, int=True),
            cv.Optional(CONF_DATA_BITS, default=8): cv.int_range(min=5, max=8),
            cv.Optional(CONF_PARITY, default="NONE"): cv.enum(
                UART_PARITY_OPTIONS, upper=True
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_at_least_one_key(CONF_TX_PIN, CONF_RX_PIN),
)


async def to_code(config):
    cg.add_global(uart_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))

    if CONF_TX_PIN in config:
        cg.add(var.set_tx_pin(config[CONF_TX_PIN]))
    if CONF_RX_PIN in config:
        cg.add(var.set_rx_pin(config[CONF_RX_PIN]))
    cg.add(var.set_rx_buffer_size(config[CONF_RX_BUFFER_SIZE]))
    if CONF_INVERT in config:
        cg.add(var.set_invert(config[CONF_INVERT]))
    cg.add(var.set_stop_bits(config[CONF_STOP_BITS]))
    cg.add(var.set_data_bits(config[CONF_DATA_BITS]))
    cg.add(var.set_parity(config[CONF_PARITY]))


def validate_device(
    name, config, item_config, baud_rate=None, require_tx=True, require_rx=True
):
    if not hasattr(config, "uart_devices"):
        config.uart_devices = {}
    devices = config.uart_devices

    uart_config = config.get_config_by_id(item_config[CONF_UART_ID])

    uart_id = uart_config[CONF_ID]
    device = devices.setdefault(uart_id, {})

    if require_tx:
        if CONF_TX_PIN not in uart_config:
            raise ValueError(f"Component {name} requires parent uart to declare tx_pin")
        if CONF_TX_PIN in device:
            raise ValueError(
                f"Component {name} cannot use the same uart.{CONF_TX_PIN} as component {device[CONF_TX_PIN]} is already using it"
            )
        device[CONF_TX_PIN] = name

    if require_rx:
        if CONF_RX_PIN not in uart_config:
            raise ValueError(f"Component {name} requires parent uart to declare rx_pin")
        if CONF_RX_PIN in device:
            raise ValueError(
                f"Component {name} cannot use the same uart.{CONF_RX_PIN} as component {device[CONF_RX_PIN]} is already using it"
            )
        device[CONF_RX_PIN] = name

    if baud_rate and uart_config[CONF_BAUD_RATE] != baud_rate:
        raise ValueError(
            f"Component {name} requires parent uart baud rate be {baud_rate}"
        )


# A schema to use for all UART devices, all UART integrations must extend this!
UART_DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_UART_ID): cv.use_id(UARTComponent),
    }
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
