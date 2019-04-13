from esphome.components import display, uart
from esphome.components.uart import UARTComponent
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_UART_ID


DEPENDENCIES = ['uart']

Nextion = display.display_ns.class_('Nextion', PollingComponent, uart.UARTDevice)
NextionRef = Nextion.operator('ref')

PLATFORM_SCHEMA = display.BASIC_DISPLAY_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(Nextion),
    cv.GenerateID(CONF_UART_ID): cv.use_variable_id(UARTComponent),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    uart_ = yield get_variable(config[CONF_UART_ID])
    rhs = App.make_nextion(uart_)
    nextion = Pvariable(config[CONF_ID], rhs)

    if CONF_LAMBDA in config:
        lambda_ = yield process_lambda(config[CONF_LAMBDA], [(NextionRef, 'it')],
                                       return_type=void)
        cg.add(nextion.set_writer(lambda_))

    display.setup_display(nextion, config)
    register_component(nextion, config)


BUILD_FLAGS = '-DUSE_NEXTION'
