import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_UART_ID

CODEOWNERS = ["@DrCoolZic"]

uart_tester_ns = cg.esphome_ns.namespace("uart_tester")
UARTTester = uart_tester_ns.class_("UARTTester", cg.PollingComponent, uart.UARTDevice)

CONF_UART_TESTER = "uart_tester"
CONF_MODE = "mode"
MULTI_CONF = True

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UARTTester),
            cv.Optional(CONF_MODE, default=0): cv.int_,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_name(str(config[CONF_UART_ID])))
    cg.add(var.set_mode(config[CONF_MODE]))
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
