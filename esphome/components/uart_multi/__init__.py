import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID
from esphome.core import coroutine

CODEOWNERS = ["@loongyh"]

DEPENDENCIES = ["uart"]

uart_multi_ns = cg.esphome_ns.namespace("uart_multi")
UARTMulti = uart_multi_ns.class_("UARTMulti", uart.UARTDevice, cg.PollingComponent)
UARTMultiDevice = uart_multi_ns.class_("UARTMultiDevice")
MULTI_CONF = True

CONF_UART_MULTI_ID = "uart_multi_id"
CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(UARTMulti)})
    .extend(cv.polling_component_schema("500ms"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


def to_code(config):
    cg.add_global(uart_multi_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    yield uart.register_uart_device(var, config)


# A schema to use for all UARTMulti devices, all UARTMulti integrations must extend this!
UART_MULTI_DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_UART_MULTI_ID): cv.use_id(UARTMulti),
    }
)


@coroutine
def register_uart_multi_device(var, config):
    parent = yield cg.get_variable(config[CONF_UART_MULTI_ID])
    cg.add(var.set_parent(parent))
    cg.add(parent.register_device(var))
