import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@NovakIrs"]
DEPENDENCIES = ["uart"]
DOMAIN = "ld2410s"
MULTI_CONF = True

CONF_LD2410S_ID = "ld2410s_id"

ld2410s_ns = cg.esphome_ns.namespace(DOMAIN)
LD2410S = ld2410s_ns.class_("LD2410S", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD2410S),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    DOMAIN,
    require_tx=True,
    require_rx=True,
    baud_rate=115200,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
