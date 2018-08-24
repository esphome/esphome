import esphomeyaml.config_validation as cv
from esphomeyaml.components import display
from esphomeyaml.components.uart import UARTComponent
from esphomeyaml.const import CONF_ID, CONF_LAMBDA, CONF_UART_ID
from esphomeyaml.helpers import App, Pvariable, add, get_variable, process_lambda

DEPENDENCIES = ['uart']

Nextion = display.display_ns.Nextion
NextionRef = Nextion.operator('ref')

PLATFORM_SCHEMA = display.BASIC_DISPLAY_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(Nextion),
    cv.GenerateID(CONF_UART_ID): cv.use_variable_id(UARTComponent),
})


def to_code(config):
    for uart in get_variable(config[CONF_UART_ID]):
        yield
    rhs = App.make_nextion(uart)
    nextion = Pvariable(config[CONF_ID], rhs)

    if CONF_LAMBDA in config:
        for lambda_ in process_lambda(config[CONF_LAMBDA], [(NextionRef, 'it')]):
            yield
        add(nextion.set_writer(lambda_))

    display.setup_display(nextion, config)


BUILD_FLAGS = '-DUSE_NEXTION'
