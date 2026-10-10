import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@NovakIrs"]

DEPENDENCIES = ["uart"]

MULTI_CONF = True

ld2410s_ns = cg.esphome_ns.namespace("ld2410s")
LD2410S = ld2410s_ns.class_("LD2410S", cg.Component, uart.UARTDevice)

CONF_LD2410S_ID = "ld2410s_id"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD2410S),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "ld2410s_uart",
    require_tx=True,
    require_rx=True,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
