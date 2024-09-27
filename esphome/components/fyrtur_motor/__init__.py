import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, cover
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]

CODEOWNERS = ["@ItsRebaseTime"]
MULTI_CONF = True

CONF_FYRTUR_MOTOR_ID = "fyrtur_motor_id"

fyrtur_motor_ns = cg.esphome_ns.namespace("fyrtur_motor")

FyrturMotorComponent = fyrtur_motor_ns.class_(
    "FyrturMotorComponent",
    cg.PollingComponent,
    uart.UARTDevice,
    cover.Cover,
)

CONFIG_SCHEMA = (
    cover.COVER_SCHEMA.extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("1s"))
    .extend(
        {
            cv.GenerateID(): cv.declare_id(FyrturMotorComponent),
        }
    )
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "fyrtur_motor",
    baud_rate=2400,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await cover.register_cover(var, config)
