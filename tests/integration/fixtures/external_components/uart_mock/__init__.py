from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
from esphome.components.const import CONF_DATA_BITS, CONF_PARITY, CONF_STOP_BITS
from esphome.components.uart import (
    CONF_RX_FULL_THRESHOLD,
    CONF_RX_TIMEOUT,
    debug_to_code,
    maybe_empty_debug,
    validate_raw_data,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_DATA,
    CONF_DEBUG,
    CONF_DELAY,
    CONF_ID,
    CONF_INTERVAL,
    CONF_RX_BUFFER_SIZE,
    CONF_TRIGGER_ID,
)
from esphome.core import ID

CODEOWNERS = ["@esphome/tests"]
MULTI_CONF = True

uart_mock_ns = cg.esphome_ns.namespace("uart_mock")
MockUartComponent = uart_mock_ns.class_(
    "MockUartComponent", uart.UARTComponent, cg.Component
)
MockUartInjectRXAction = uart_mock_ns.class_(
    "MockUartInjectRXAction", automation.Action
)
MockUartTXTrigger = uart_mock_ns.class_(
    "MockUartTXTrigger",
    automation.Trigger.template(cg.std_vector.template(cg.uint8)),
)

CONF_INJECTIONS = "injections"
CONF_RESPONSES = "responses"
CONF_INJECT_RX = "inject_rx"
CONF_EXPECT_TX = "expect_tx"
CONF_PERIODIC_RX = "periodic_rx"
CONF_ON_TX = "on_tx"

UART_PARITY_OPTIONS = {
    "NONE": uart.UARTParityOptions.UART_CONFIG_PARITY_NONE,
    "EVEN": uart.UARTParityOptions.UART_CONFIG_PARITY_EVEN,
    "ODD": uart.UARTParityOptions.UART_CONFIG_PARITY_ODD,
}

INJECTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_INJECT_RX): [cv.hex_uint8_t],
        cv.Optional(CONF_DELAY, default="0ms"): cv.positive_time_period_milliseconds,
    }
)

CONFIG_INJECT_RX_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(MockUartComponent),
        cv.Required("data"): cv.templatable(validate_raw_data),
    },
    key=CONF_DATA,
)


RESPONSE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_EXPECT_TX): [cv.hex_uint8_t],
        cv.Required(CONF_INJECT_RX): [cv.hex_uint8_t],
    }
)

PERIODIC_RX_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DATA): [cv.hex_uint8_t],
        cv.Required(CONF_INTERVAL): cv.positive_time_period_milliseconds,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MockUartComponent),
        cv.Required(CONF_BAUD_RATE): cv.int_range(min=1),
        cv.Optional(CONF_RX_BUFFER_SIZE, default=256): cv.validate_bytes,
        cv.Optional(CONF_RX_FULL_THRESHOLD, default=10): cv.int_range(min=1, max=120),
        cv.Optional(CONF_RX_TIMEOUT, default=2): cv.int_range(min=0, max=92),
        cv.Optional(CONF_STOP_BITS, default=1): cv.one_of(1, 2, int=True),
        cv.Optional(CONF_DATA_BITS, default=8): cv.int_range(min=5, max=8),
        cv.Optional(CONF_PARITY, default="NONE"): cv.enum(
            UART_PARITY_OPTIONS, upper=True
        ),
        cv.Optional(CONF_INJECTIONS, default=[]): cv.ensure_list(INJECTION_SCHEMA),
        cv.Optional(CONF_RESPONSES, default=[]): cv.ensure_list(RESPONSE_SCHEMA),
        cv.Optional(CONF_PERIODIC_RX, default=[]): cv.ensure_list(PERIODIC_RX_SCHEMA),
        cv.Optional(CONF_ON_TX): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MockUartTXTrigger),
            }
        ),
        cv.Optional(CONF_DEBUG): maybe_empty_debug,
    }
).extend(cv.COMPONENT_SCHEMA)


@automation.register_action(
    "uart_mock.inject_rx", MockUartInjectRXAction, CONFIG_INJECT_RX_SCHEMA
)
async def inject_rx_to_code(config, action_id, template_arg, args):
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


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))
    cg.add(var.set_rx_buffer_size(config[CONF_RX_BUFFER_SIZE]))
    cg.add(var.set_rx_full_threshold(config[CONF_RX_FULL_THRESHOLD]))
    cg.add(var.set_rx_timeout(config[CONF_RX_TIMEOUT]))
    cg.add(var.set_stop_bits(config[CONF_STOP_BITS]))
    cg.add(var.set_data_bits(config[CONF_DATA_BITS]))
    cg.add(var.set_parity(config[CONF_PARITY]))

    for injection in config[CONF_INJECTIONS]:
        rx_data = injection[CONF_INJECT_RX]
        delay_ms = injection[CONF_DELAY]
        cg.add(var.add_injection(rx_data, delay_ms))

    for response in config[CONF_RESPONSES]:
        tx_data = response[CONF_EXPECT_TX]
        rx_data = response[CONF_INJECT_RX]
        cg.add(var.add_response(tx_data, rx_data))

    for periodic in config[CONF_PERIODIC_RX]:
        data = periodic[CONF_DATA]
        interval = periodic[CONF_INTERVAL]
        cg.add(var.add_periodic_rx(data, interval))

    for conf in config.get(CONF_ON_TX, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.std_vector.template(cg.uint8), "data")], conf
        )

    if CONF_DEBUG in config:
        await debug_to_code(config[CONF_DEBUG], var)
