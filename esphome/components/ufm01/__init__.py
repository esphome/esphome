import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@ljungqvist"]

MULTI_CONF = True

DEPENDENCIES = ["uart"]

ufm01_ns = cg.esphome_ns.namespace("ufm01")
UFM01Component = ufm01_ns.class_("UFM01Component", uart.UARTDevice, cg.Component)

CONF_UFM01_ID = "ufm01_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UFM01Component),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "ufm01",
    require_tx=True,
    require_rx=True,
    baud_rate=2400,
    parity="EVEN",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
