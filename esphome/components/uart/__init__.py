from esphome import pins
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_BAUD_RATE, CONF_ID, CONF_RX_PIN, CONF_TX_PIN, CONF_UART_ID
from esphome.core import CORE, coroutine

uart_ns = cg.esphome_ns.namespace('uart')
UARTComponent = uart_ns.class_('UARTComponent', cg.Component)
UARTDevice = uart_ns.class_('UARTDevice')
MULTI_CONF = True


def validate_rx_pin(value):
    value = pins.input_pin(value)
    if CORE.is_esp8266 and value >= 16:
        raise cv.Invalid("Pins GPIO16 and GPIO17 cannot be used as RX pins on ESP8266.")
    return value


CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(UARTComponent),
    cv.Optional(CONF_TX_PIN): pins.output_pin,
    cv.Optional(CONF_RX_PIN): validate_rx_pin,
    cv.Required(CONF_BAUD_RATE): cv.positive_int,
}).extend(cv.COMPONENT_SCHEMA), cv.has_at_least_one_key(CONF_TX_PIN, CONF_RX_PIN))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_BAUD_RATE])
    yield cg.register_component(var, config)

    if CONF_TX_PIN in config:
        cg.add(var.set_tx_pin(config[CONF_TX_PIN]))
    if CONF_RX_PIN in config:
        cg.add(var.set_rx_pin(config[CONF_RX_PIN]))


UART_DEVICE_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_UART_ID): cv.use_variable_id(UARTComponent),
})


@coroutine
def register_uart_device(var, config):
    parent = yield cg.get_variable(config[CONF_UART_ID])
    cg.add(var.set_uart_parent(parent))
