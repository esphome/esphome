import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]

CODEOWNERS = ["@ItsRebaseTime"]
MULTI_CONF = True

CONF_FYRTUR_MOTOR_ID = "fyrtur_motor_id"

fyrtur_motor_ns = cg.esphome_ns.namespace("fyrtur_motor")

FyrturMotorComponent = fyrtur_motor_ns.class_(
    "FyrturMotorComponent", cg.PollingComponent, uart.UARTDevice
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(FyrturMotorComponent),
    }
)

CONFIG_SCHEMA = cv.All(
    CONFIG_SCHEMA.extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("5s"))
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "fyrtur_motor",
    require_tx=True,
    require_rx=True,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
