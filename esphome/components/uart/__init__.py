import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins, automation
from esphome.const import CONF_BAUD_RATE, CONF_ID, CONF_RX_PIN, CONF_TX_PIN, CONF_UART_ID, CONF_DATA
from esphome.core import CORE, coroutine
from esphome.py_compat import text_type, binary_type, char_to_byte

uart_ns = cg.esphome_ns.namespace('uart')
UARTComponent = uart_ns.class_('UARTComponent', cg.Component)
UARTDevice = uart_ns.class_('UARTDevice')
UARTWriteAction = uart_ns.class_('UARTWriteAction', automation.Action)
MULTI_CONF = True


def validate_raw_data(value):
    if isinstance(value, text_type):
        return value.encode('utf-8')
    if isinstance(value, str):
        return value
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid("data must either be a string wrapped in quotes or a list of bytes")


def validate_rx_pin(value):
    value = pins.input_pin(value)
    if CORE.is_esp8266 and value >= 16:
        raise cv.Invalid("Pins GPIO16 and GPIO17 cannot be used as RX pins on ESP8266.")
    return value


CONF_STOP_BITS = 'stop_bits'
CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(UARTComponent),
    cv.Required(CONF_BAUD_RATE): cv.int_range(min=1),
    cv.Optional(CONF_TX_PIN): pins.output_pin,
    cv.Optional(CONF_RX_PIN): validate_rx_pin,
    cv.Optional(CONF_STOP_BITS, default=1): cv.one_of(1, 2, int=True),
}).extend(cv.COMPONENT_SCHEMA), cv.has_at_least_one_key(CONF_TX_PIN, CONF_RX_PIN))


def to_code(config):
    cg.add_global(uart_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))

    if CONF_TX_PIN in config:
        cg.add(var.set_tx_pin(config[CONF_TX_PIN]))
    if CONF_RX_PIN in config:
        cg.add(var.set_rx_pin(config[CONF_RX_PIN]))
    cg.add(var.set_stop_bits(config[CONF_STOP_BITS]))


# A schema to use for all UART devices, all UART integrations must extend this!
UART_DEVICE_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_UART_ID): cv.use_id(UARTComponent),
})


@coroutine
def register_uart_device(var, config):
    """Register a UART device, setting up all the internal values.

    This is a coroutine, you need to await it with a 'yield' expression!
    """
    parent = yield cg.get_variable(config[CONF_UART_ID])
    cg.add(var.set_uart_parent(parent))


@automation.register_action('uart.write', UARTWriteAction, cv.maybe_simple_value({
    cv.GenerateID(): cv.use_id(UARTComponent),
    cv.Required(CONF_DATA): cv.templatable(validate_raw_data),
}, key=CONF_DATA))
def uart_write_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    data = config[CONF_DATA]
    if isinstance(data, binary_type):
        data = [char_to_byte(x) for x in data]

    if cg.is_template(data):
        templ = yield cg.templatable(data, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_data_template(templ))
    else:
        cg.add(var.set_data_static(data))
    yield var
