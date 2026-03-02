import esphome.codegen as cg
from esphome.components import uart
from esphome.components.uart import CONF_DATA_BITS, CONF_PARITY, CONF_STOP_BITS
import esphome.config_validation as cv
from esphome.const import CONF_BAUD_RATE, CONF_DATA, CONF_DELAY, CONF_ID, CONF_INTERVAL

CODEOWNERS = ["@esphome/tests"]
MULTI_CONF = True

uart_mock_ns = cg.esphome_ns.namespace("uart_mock")
MockUartComponent = uart_mock_ns.class_(
    "MockUartComponent", uart.UARTComponent, cg.Component
)

CONF_INJECTIONS = "injections"
CONF_RESPONSES = "responses"
CONF_INJECT_RX = "inject_rx"
CONF_EXPECT_TX = "expect_tx"
CONF_PERIODIC_RX = "periodic_rx"

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
        cv.Optional(CONF_STOP_BITS, default=1): cv.one_of(1, 2, int=True),
        cv.Optional(CONF_DATA_BITS, default=8): cv.int_range(min=5, max=8),
        cv.Optional(CONF_PARITY, default="NONE"): cv.enum(
            UART_PARITY_OPTIONS, upper=True
        ),
        cv.Optional(CONF_INJECTIONS, default=[]): cv.ensure_list(INJECTION_SCHEMA),
        cv.Optional(CONF_RESPONSES, default=[]): cv.ensure_list(RESPONSE_SCHEMA),
        cv.Optional(CONF_PERIODIC_RX, default=[]): cv.ensure_list(PERIODIC_RX_SCHEMA),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))
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
