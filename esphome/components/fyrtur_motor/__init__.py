import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]

CODEOWNERS = ["@ItsRebaseTime"]

CONF_FYRTUR_MOTOR_ID = "fyrtur_motor_id"

fyrtur_motor_ns = cg.esphome_ns.namespace("fyrtur_motor_ns")
FyrturMotorComponent = fyrtur_motor_ns.class_(
    "FyrturMotorComponent", cg.PollingComponent, uart.UARTDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FyrturMotorComponent),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("30s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
