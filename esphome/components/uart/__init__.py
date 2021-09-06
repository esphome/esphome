from typing import Optional

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
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
):
    def validate_baud_rate(value):
        if value != baud_rate:
            raise cv.Invalid(
                f"Component {name} required baud rate {baud_rate} for the uart bus"
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
