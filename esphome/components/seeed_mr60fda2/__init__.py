import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@limengdu"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

mr60fda2_ns = cg.esphome_ns.namespace("seeed_mr60fda2")

MR60FDA2Component = mr60fda2_ns.class_(
    "MR60FDA2Component", cg.Component, uart.UARTDevice
)

CONF_MR60FDA2_ID = "mr60fda2_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MR60FDA2Component),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "seeed_mr60fda2",
    require_tx=True,
    require_rx=True,
    baud_rate=115200,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
